#ifndef FEASIBILITY_MANAGER_H
#define FEASIBILITY_MANAGER_H

#include <chrono>

#include "problem.hpp"

#include "reconfiguration/options.hpp"
#include "reconfiguration/result_printer.hpp"

#include "simple_bounds.hpp"
#include "load.hpp"
#include "interval.hpp"
#include "z3.hpp"
#include "cplex.hpp"
#include "uppaal.hpp"
#include "node.hpp"
#include "from_scratch.hpp"

namespace NP::Feasibility {
	template<class Time> static bool run_necessary_tests(const NP::Scheduling_problem<Time> &problem) {
		const auto bounds = compute_simple_bounds(problem);
		if (bounds.definitely_infeasible) {
			print_infeasible_bounds_results(bounds, problem);
			return false;
		}

		Load_test<Time> load_test(problem, bounds);
		while (load_test.next());
		if (print_feasibility_load_test_results(load_test)) return false;

		Interval_test<Time> interval_test(problem, bounds);
		while (interval_test.next());
		if (print_feasibility_interval_test_results(interval_test, problem)) return false;
		return true;
	}

	template<class Time> static void print_schedule(
		const NP::Scheduling_problem<Time> &problem,
		const std::vector<Job_index> &safe_path,
		const char *error_message
	) {
		if (safe_path.empty()) return;
		if (safe_path.size() != problem.jobs.size()) throw std::runtime_error("invalid safe_path size; this is a bug!");

		const auto bounds = compute_simple_bounds(problem);
		const auto predecessor_mapping = create_predecessor_mapping(problem);
		std::vector<Time> start_times(safe_path.size(), -1);
		Active_node<Time> node(problem.num_processors);
		for (const Job_index job_index : safe_path) {
			const auto &job = problem.jobs[job_index];
			start_times[job_index] = node.predict_start_time(job, predecessor_mapping);
			node.schedule(job, bounds, predecessor_mapping);
			if (node.has_missed_deadline()) throw std::runtime_error(error_message);
		}

		std::cout << "This problem is feasible. I will present a schedule:" << std::endl;
		for (const Job_index job_index : safe_path) {
			const Time finish_time = start_times[job_index] + problem.jobs[job_index].maximal_exec_time();
			std::cout << " - start " << problem.jobs[job_index].get_id() << " at time " << start_times[job_index] <<
					" and it should finish no later than " << finish_time << std::endl;
		}
	}

	template<class Time> static void run_exact_test(
		const NP::Scheduling_problem<Time> &problem,
		const NP::Reconfiguration::SafeSearchOptions options,
		const int num_threads, const bool should_print_schedule
	) {
		if (!run_necessary_tests(problem)) return;

		const auto bounds = compute_simple_bounds(problem);
		const auto predecessor_mapping = create_predecessor_mapping(problem);
		const auto start_time = std::chrono::high_resolution_clock::now();
		const auto safe_path = search_for_safe_job_ordering(problem, bounds, predecessor_mapping, options, num_threads, true);
		const auto end_time = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::ratio<1, 1>> spent_real_time = end_time - start_time;
		std::cout << "I found a safe job ordering after " << spent_real_time.count() << " seconds!" << std::endl;
		if (should_print_schedule) print_schedule(problem, safe_path, "invalid path? this is a bug!");
	}

	static void run_z3(const NP::Scheduling_problem<dtime_t> &problem, const bool should_print_schedule, int model) {
		const auto bounds = compute_simple_bounds(problem);
		if (bounds.definitely_infeasible) {
			print_infeasible_bounds_results(bounds, problem);
			return;
		}

		const auto safe_path = find_safe_job_ordering_with_z3(problem, bounds, model);
		if (should_print_schedule) print_schedule(problem, safe_path, "invalid z3 path? this is a bug!");
	}

	static void run_cplex(const NP::Scheduling_problem<dtime_t> &problem, const bool should_print_schedule) {
		const auto bounds = compute_simple_bounds(problem);
		if (bounds.definitely_infeasible) {
			print_infeasible_bounds_results(bounds, problem);
			return;
		}

		const auto safe_path = find_safe_job_ordering_with_cplex(problem, bounds);
		if (should_print_schedule) print_schedule(problem, safe_path, "invalid cplex path? this is a bug!");
	}

	static void run_uppaal(const NP::Scheduling_problem<dtime_t> &problem, const bool should_print_schedule) {
		const auto bounds = compute_simple_bounds(problem);
		if (bounds.definitely_infeasible) {
			print_infeasible_bounds_results(bounds, problem);
			return;
		}

		find_safe_job_ordering_with_uppaal(problem, bounds);
	}
}

#endif
