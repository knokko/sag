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
#include "cut_loop.hpp"
#include "transitivity_constraint_minimizer.hpp"
#include "tail_constraint_minimizer.hpp"
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

	template<class Time> static std::vector<Job_index> find_safe_job_ordering(
		const Options &options, const NP::Scheduling_problem<Time> &problem
	) {
		const auto bounds = Feasibility::compute_simple_bounds(problem);
		if (bounds.definitely_infeasible) {
			print_infeasible_bounds_results(bounds, problem);
			return {};
		}

		Rating_graph rating_graph;
		Agent_rating_graph<Time>::generate(problem, rating_graph);

		if (rating_graph.nodes[0].get_rating() == 1.0f) {
			std::cout << "The given problem is already schedulable using our scheduler." << std::endl;
			return {};
		}

		std::cout << "The given problem seems to be unschedulable using our scheduler, and the root rating is " << rating_graph.nodes[0].get_rating() << "." << std::endl;

		if (problem.jobs.size() < 15) {
			rating_graph.generate_full_dot_file("nptest.dot", problem, {});
		}

		{
			Feasibility::Load_test<Time> load_test(problem, bounds);
			while (load_test.next());
			if (print_feasibility_load_test_results(load_test)) return {};
		}

		{
			Feasibility::Interval_test<Time> interval_test(problem, bounds);
			while (interval_test.next());
			if (print_feasibility_interval_test_results(interval_test, problem)) return {};
		}

		std::vector<Job_index> safe_path;
		if (rating_graph.nodes[0].get_rating() > 0.0) {
			Feasibility::Feasibility_graph<Time> feasibility_graph(rating_graph);
			const auto predecessor_mapping = Feasibility::create_predecessor_mapping(problem);
			feasibility_graph.explore_forward(problem, bounds, predecessor_mapping);
			feasibility_graph.explore_backward();

			if (feasibility_graph.is_node_feasible(0)) safe_path = feasibility_graph.create_safe_path(problem);
			else safe_path = feasibility_graph.try_to_find_random_safe_path(problem, options.max_feasibility_graph_attempts, false);
		} else std::cout << "Since the root rating is 0,";

		if (safe_path.empty()) {
			std::cout << " I will need to create a safe job ordering from scratch, which may take seconds or centuries..." << std::endl;
			std::cout << "You should press Control + C if you run out of patience!" << std::endl;
			const auto predecessor_mapping = Feasibility::create_predecessor_mapping(problem);
			safe_path = Feasibility::search_for_safe_job_ordering(problem, bounds, predecessor_mapping, 50, true);
		}

		return safe_path;
	}

	template<class Time> static void inner_reconfigure(Options &options, NP::Scheduling_problem<Time> &problem) {
		const size_t num_original_constraints = problem.prec.size();
		const std::vector<Job_index> safe_path = find_safe_job_ordering(options, problem);
		if (safe_path.empty()) return;

		Cut_loop<Time> cut_loop(problem, safe_path);
		cut_loop.cut_until_finished(true, options.max_cuts_per_iteration);

		std::cout << (problem.prec.size() - num_original_constraints) << " dispatch ordering constraints were added, let's try to minimize that..." << std::endl;
		auto transitivity_minimizer = Transitivity_constraint_minimizer<Time>(problem, num_original_constraints);
		transitivity_minimizer.remove_redundant_constraints();

		auto space = Global::State_space<Time>::explore(problem, {}, nullptr);
		if (!space->is_schedulable()) throw std::runtime_error("Transitivity analysis failed; this should not be possible!");
		delete space;

		std::cout << (problem.prec.size() - num_original_constraints) << " remain after transitivity analysis; let's minimize further..." << std::endl;

		Tail_constraint_minimizer<Time> tail_minimizer(problem, num_original_constraints);
		tail_minimizer.remove_constraints_until_finished(options.num_threads, true);

		space = Global::State_space<Time>::explore(problem, {}, nullptr);
		if (!space->is_schedulable()) throw std::runtime_error("Tail reduction failed; this should not be possible!");
		delete space;

		std::cout << (problem.prec.size() - num_original_constraints) << " remain after tail analysis; let's minimize further..." << std::endl;

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
}
#endif
