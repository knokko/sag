#ifndef FEASIBILITY_MANAGER_H
#define FEASIBILITY_MANAGER_H

#include "problem.hpp"

#include "reconfiguration/result_printer.hpp"

#include "simple_bounds.hpp"
#include "load.hpp"
#include "interval.hpp"
#include "z3.hpp"
#include "node.hpp"

namespace NP::Feasibility {
	template<class Time> static void run_necessary_tests(const NP::Scheduling_problem<Time> &problem) {
		const auto bounds = compute_simple_bounds(problem);
		if (bounds.definitely_infeasible) {
			print_infeasible_bounds_results(bounds, problem);
			return;
		}

		Load_test<Time> load_test(problem, bounds);
		while (load_test.next());
		if (print_feasibility_load_test_results(load_test)) return;

		Interval_test<Time> interval_test(problem, bounds);
		while (interval_test.next());
		if (print_feasibility_interval_test_results(interval_test, problem)) return;

		std::cout << "This problem could be feasible" << std::endl;
	}

	template<class Time> static void run_z3(const NP::Scheduling_problem<Time> &problem) {
		const auto bounds = compute_simple_bounds(problem);
		if (bounds.definitely_infeasible) {
			print_infeasible_bounds_results(bounds, problem);
			return;
		}

		const auto safe_path = find_safe_job_ordering_with_z3(problem, bounds);
		if (safe_path.empty()) return;
		if (safe_path.size() != problem.jobs.size()) throw std::runtime_error("invalid safe_path size; this is a bug!");

		const auto predecessor_mapping = create_predecessor_mapping(problem);
		std::vector<Time> start_times(safe_path.size(), -1);
		Active_node<Time> node(problem.num_processors);
		for (const Job_index job_index : safe_path) {
			const auto &job = problem.jobs[job_index];
			start_times[job_index] = node.predict_start_time(job, predecessor_mapping);
			node.schedule(job, bounds, predecessor_mapping);
			if (node.has_missed_deadline()) throw std::runtime_error("z3 path seems invalid; this is a bug!");
		}

		std::cout << "This problem is feasible. I will present a schedule:" << std::endl;
		for (const Job_index job_index : safe_path) {
			const Time finish_time = start_times[job_index] + problem.jobs[job_index].maximal_exec_time();
			std::cout << " - start " << problem.jobs[job_index].get_id() << " at time " << start_times[job_index] <<
					" and it should finish no later than " << finish_time << std::endl;
		}
	}
}

#endif
