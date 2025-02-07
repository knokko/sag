#ifndef RECONFIGURATION_MANAGER_H
#define RECONFIGURATION_MANAGER_H

#include <thread>
#include <mutex>

#include "clock.hpp"
#include "dump.hpp"
#include "global/space.hpp"

#include "options.hpp"
#include "rating_graph.hpp"
#include "graph_cutter.hpp"
#include "cut_enforcer.hpp"
#include "transitivity_constraint_minimizer.hpp"
#include "trial_constraint_minimizer.hpp"
#include "result_printer.hpp"

#include "feasibility/graph.hpp"
#include "feasibility/simple_bounds.hpp"
#include "feasibility/load.hpp"
#include "feasibility/interval.hpp"
#include "feasibility/z3.hpp"
#include "feasibility/from_scratch.hpp"

namespace NP::Reconfiguration {

	template<class Time> static void run(Options &options, NP::Scheduling_problem<Time> &problem) {
		Processor_clock cpu_time;
		cpu_time.start();

		inner_reconfigure(options, problem);

		double spent_time = cpu_time.stop();
		std::cout << "Reconfiguration took " << spent_time << " seconds and consumed " <<
				(get_peak_memory_usage() / 1024) << "MB of memory" << std::endl;
	}

	template<class Time> static void launch_trial_and_error_thread(
		std::shared_ptr<Scheduling_problem<Time>> problem, size_t num_original_constraints, std::shared_ptr<std::mutex> lock, int num_threads
	) {
		auto trial_minimizer = Trial_constraint_minimizer<Time>(problem, num_original_constraints, lock, num_threads);
		trial_minimizer.repeatedly_try_to_remove_random_constraints();
	}

	template<class Time> static void inner_reconfigure(Options &options, NP::Scheduling_problem<Time> &problem) {
		const size_t num_original_constraints = problem.prec.size();
		const auto bounds = Feasibility::compute_simple_bounds(problem);
		if (bounds.definitely_infeasible) {
			print_infeasible_bounds_results(bounds, problem);
			return;
		}

		if (!inner_reconfigure_first_try(options, problem, bounds, num_original_constraints)) {
			int max_num_cuts = 100;
			size_t older_constraint_count = num_original_constraints;
			size_t old_constraint_count = problem.prec.size();
			bool failed_last_attempt = false;
			while (true) {
				int raw_result = inner_reconfigure_iteration(options, problem, bounds, num_original_constraints, failed_last_attempt ? 1 : max_num_cuts);
				std::cout << "raw result is " << raw_result << " and old count is " << old_constraint_count << " and older is " << older_constraint_count << std::endl;

				if (raw_result == 2) break;
				if (raw_result == 0) {
					failed_last_attempt = true;
					problem.prec.resize(older_constraint_count, Precedence_constraint<Time>(problem.jobs[0].get_id(), problem.jobs[1].get_id(), Interval<Time>()));
					old_constraint_count = older_constraint_count;
					max_num_cuts /= 10;
					if (max_num_cuts < 1) max_num_cuts = 1;
				} else {
					if (max_num_cuts < problem.jobs.size()) max_num_cuts *= 2;
					failed_last_attempt = false;
					older_constraint_count = old_constraint_count;
					old_constraint_count = problem.prec.size();
				}
			}
		}

		if (num_original_constraints == problem.prec.size()) {
			std::cout << "The root rating is 0... tough one!" << std::endl;
			std::cout << "I must search for a safe job ordering, which may take seconds or centuries..." << std::endl;
			std::cout << "You should press Control + C if you run out of patience!" << std::endl;
			const auto predecessor_mapping = Feasibility::create_predecessor_mapping(problem);
			const auto safe_job_ordering = Feasibility::search_for_safe_job_ordering(problem, bounds, predecessor_mapping, 50, true);
			Feasibility::enforce_safe_job_ordering(problem, safe_job_ordering);
			std::cout << "I found a safe job ordering, which requires " << (problem.prec.size() - num_original_constraints) << " additional constraints" << std::endl;

			// Sanity check
			auto space = Global::State_space<Time>::explore(problem, {}, nullptr);
			if (!space->is_schedulable()) {
				Rating_graph rating_graph;
				Agent_rating_graph<Time>::generate(problem, rating_graph);
				rating_graph.generate_full_dot_file("scratch.dot", problem, {}, false);
				if constexpr(std::is_same<Time, dtime_t>::value) {
					dump_problem("scratch.csv", "scratch.prec.csv", problem);
				}
				throw std::runtime_error("Enforcing safe job ordering failed; this should be impossible!");
			}
			delete space;
		}

		std::cout << (problem.prec.size() - num_original_constraints) << " dispatch ordering constraints were added, let's try to minimize that..." << std::endl;
		auto transitivity_minimizer = Transitivity_constraint_minimizer<Time>(problem, num_original_constraints);
		transitivity_minimizer.remove_redundant_constraints();

		auto space = Global::State_space<Time>::explore(problem, {}, nullptr);
		if (!space->is_schedulable()) throw std::runtime_error("Transitivity analysis failed; this should not be possible!");
		delete space;

		std::cout << (problem.prec.size() - num_original_constraints) << " remain after transitivity analysis; let's minimize further..." << std::endl;

		auto shared_problem = std::make_shared<Scheduling_problem<Time>>(problem);
		auto lock = std::make_shared<std::mutex>();
		std::vector<std::thread> trial_threads;
		for (size_t counter = 0; counter < options.num_threads; counter++) {
			trial_threads.push_back(std::thread(launch_trial_and_error_thread<Time>, shared_problem, num_original_constraints, lock, options.num_threads));
		}
		for (auto &trial_thread : trial_threads) trial_thread.join();
		problem = *shared_problem;

		space = Global::State_space<Time>::explore(problem, {}, nullptr);
		if (!space->is_schedulable()) throw std::runtime_error("Trial & error 'analysis' failed; this should not be possible!");
		delete space;

		std::cout << (problem.prec.size() - num_original_constraints) << " remain after trial & error 'analysis'." << std::endl;
		print_fixing_changes(problem, num_original_constraints);
	}

	template<class Time> static bool inner_reconfigure_first_try(
		Options &options, NP::Scheduling_problem<Time> &problem, const Feasibility::Simple_bounds<Time> &bounds, size_t num_original_constraints
	) {
		Rating_graph rating_graph;
		Agent_rating_graph<Time>::generate(problem, rating_graph);

		if (rating_graph.nodes[0].get_rating() == 1.0f) {
			std::cout << "The given problem is already schedulable using our scheduler." << std::endl;
			return true;
		}

		std::cout << "The given problem seems to be unschedulable using our scheduler, and the root rating is " << rating_graph.nodes[0].get_rating() << "." << std::endl;

		if (problem.jobs.size() < 15) {
			rating_graph.generate_full_dot_file("nptest.dot", problem, {});
		}

		{
			Feasibility::Load_test<Time> load_test(problem, bounds);
			while (load_test.next());
			if (print_feasibility_load_test_results(load_test)) return true;
		}

		{
			Feasibility::Interval_test<Time> interval_test(problem, bounds);
			while (interval_test.next());
			if (print_feasibility_interval_test_results(interval_test, problem)) return true;
		}

		if (rating_graph.nodes[0].get_rating() == 0.0) return true;

		std::vector<Rating_graph_cut> cuts;
		{
			Feasibility::Feasibility_graph<Time> feasibility_graph(rating_graph);
			const auto predecessor_mapping = Feasibility::create_predecessor_mapping(problem);
			feasibility_graph.explore_forward(problem, bounds, predecessor_mapping);
			feasibility_graph.explore_backward();

			cuts = cut_rating_graph(rating_graph, feasibility_graph);
		}

		print_cuts(cuts, problem);

		for (const auto &cut : cuts) {
			if (cut.safe_jobs.empty()) {
				std::cout << "Found unfixable cut; feasibility graph failed" << std::endl;
				return true;
			}
		}

		enforce_cuts(problem, rating_graph, cuts, bounds);

		const auto space = Global::State_space<Time>::explore(problem, {}, nullptr);
		bool worked = space->is_schedulable();
		delete space;

		if (!worked) {
			std::cout << "It looks like 1 iteration wasn't enough. Trying more iterations..." << std::endl;
			return false;
		}

		return true;
	}

	template<class Time> static int inner_reconfigure_iteration(
		Options &options, NP::Scheduling_problem<Time> &problem, const Feasibility::Simple_bounds<Time> &bounds,
		size_t num_original_constraints, size_t max_num_cuts
	) {
		std::cout << "begin next iteration with max " << max_num_cuts << " cuts" << std::endl;
		const auto new_bounds = Feasibility::compute_simple_bounds(problem);
		if (new_bounds.has_precedence_cycle) std::cout << "Found precedence cycle" << std::endl;
		if (new_bounds.definitely_infeasible) {
			std::cout << "Became infeasible" << std::endl;
			return 0;
		}
		Rating_graph rating_graph;
		Agent_rating_graph<Time>::generate(problem, rating_graph);

		if (rating_graph.nodes[0].get_rating() == 1.0f) {
			return 2;
		}

		if (rating_graph.nodes[0].get_rating() == 0.0f) {
			std::cout << "root rating became 0" << std::endl;
			return 0;
		}

		std::vector<Rating_graph_cut> cuts;
		{
			Feasibility::Feasibility_graph<Time> feasibility_graph(rating_graph);
			const auto predecessor_mapping = Feasibility::create_predecessor_mapping(problem);
			feasibility_graph.explore_forward(problem, bounds, predecessor_mapping);
			feasibility_graph.explore_backward();

			cuts = cut_rating_graph(rating_graph, feasibility_graph);
		}
		std::cout << "There are " << cuts.size() << " cuts left" << std::endl;

		for (const auto &cut : cuts) {
			if (cut.safe_jobs.empty()) std::runtime_error("Encountered unfixable cut in a later iteration");
		}

		cuts.resize(std::min(cuts.size(), max_num_cuts));
		enforce_cuts(problem, rating_graph, cuts, bounds);

		return 1;
	}
}
#endif
