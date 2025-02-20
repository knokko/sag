#ifndef RECONFIGURATION_MANAGER_H
#define RECONFIGURATION_MANAGER_H

#include <chrono>

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
		auto start_time = std::chrono::high_resolution_clock::now();

		inner_reconfigure(options, problem);

		double spent_cpu_time = cpu_time.stop();
		auto stop_time = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::ratio<1, 1>> spent_real_time = stop_time - start_time;
		std::cout << "Reconfiguration took " << spent_real_time.count() <<
				" seconds (" << spent_cpu_time << " CPU seconds) and consumed " <<
				(get_peak_memory_usage() / 1024) << "MB of memory" << std::endl;
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

		std::cout << "The given problem seems to be unschedulable using our scheduler, and the rating of the root node is " << rating_graph.nodes[0].get_rating() << "." << std::endl;

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

			if (feasibility_graph.is_node_feasible(0)) {
				safe_path = feasibility_graph.create_safe_path(problem);
			} else {
				safe_path = feasibility_graph.try_to_find_random_safe_path(problem, options.max_feasibility_graph_attempts, false);
				if (safe_path.empty()) std::cout << "Since the root node is unsafe,";
			}
		} else std::cout << "Since the rating of the root node is 0,";

		if (safe_path.empty()) {
			std::cout << " I will need to create a safe job ordering from scratch, which may take seconds or centuries..." << std::endl;
			std::cout << "You should press Control + C if you run out of patience!" << std::endl;
			const auto predecessor_mapping = Feasibility::create_predecessor_mapping(problem);
			safe_path = Feasibility::search_for_safe_job_ordering(
				problem, bounds, predecessor_mapping, 50, options.num_threads, true
			);
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

		std::cout << (problem.prec.size() - num_original_constraints) << " remain after transitivity analysis; let's minimize further ";

		if (options.use_random_analysis) {
			std::cout << "using random trial-and-error..." << std::endl;
			Trial_constraint_minimizer<Time> trial_minimizer(problem, num_original_constraints, options.num_threads, true);
			trial_minimizer.repeatedly_try_to_remove_random_constraints();
		} else {
			std::cout << "using tail trial-and-error..." << std::endl;
			Tail_constraint_minimizer<Time> tail_minimizer(problem, num_original_constraints);
			tail_minimizer.remove_constraints_until_finished(options.num_threads, true);
		}

		space = Global::State_space<Time>::explore(problem, {}, nullptr);
		if (!space->is_schedulable()) throw std::runtime_error("Trial & error failed; this should not be possible!");
		delete space;

		std::cout << (problem.prec.size() - num_original_constraints) << " remain after trial & error." << std::endl;
		print_fixing_changes(problem, num_original_constraints);
	}
}
#endif
