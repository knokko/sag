#ifndef RECONFIGURATION_RESULT_PRINTER_H
#define RECONFIGURATION_RESULT_PRINTER_H

#include "rating_graph.hpp"
#include "feasibility/simple_bounds.hpp"
#include "feasibility/load.hpp"
#include "feasibility/interval.hpp"

namespace NP::Reconfiguration {

	template<class Time> static void print_infeasible_bounds_results(
		const Feasibility::Simple_bounds<Time> &bounds, const Scheduling_problem<Time> &problem
	) {
		if (bounds.has_precedence_cycle) {
			std::cout << "The given problem is infeasible because it has a precedence constraint cycle:"
						<< std::endl;
			for (size_t index = 1; index < bounds.problematic_chain.size(); index++) {
				std::cout << " - " << problem.jobs[bounds.problematic_chain[index]].get_id() << " depends on " <<
							problem.jobs[bounds.problematic_chain[index - 1]].get_id() << std::endl;
			}
		} else {
			std::cout << "The given problem is infeasible because a deadline will be missed when all jobs "
							"arrive at their latest arrival time and run for their worst-case execution time:"
						<< std::endl;
			const auto &first_job = problem.jobs[bounds.problematic_chain[0]];
			std::cout << " - " << first_job.get_id() << " arrives at " << first_job.latest_arrival() <<
						" and finishes at " << (first_job.latest_arrival() + first_job.maximal_exec_time())
						<< std::endl;
			for (size_t index = 1; index < bounds.problematic_chain.size(); index++) {
				const auto &next_job = problem.jobs[bounds.problematic_chain[index]];
				const auto &previous_job = problem.jobs[bounds.problematic_chain[index - 1]];
				std::cout << " - Since " << next_job.get_id() << " depends on " << previous_job.get_id()
							<< ", it can only start at ";
				Time next_start_time = bounds.earliest_pessimistic_start_times[next_job.get_job_index()];
				std::cout << next_start_time;
				if (next_start_time > bounds.earliest_pessimistic_start_times[previous_job.get_job_index()] +
										previous_job.maximal_exec_time()) {
					std::cout << " (due to suspension)";
				}
				std::cout << ", and finishes at " << (next_start_time + next_job.maximal_exec_time()) << std::endl;
			}

			const auto &last_job = problem.jobs[bounds.problematic_chain[bounds.problematic_chain.size() - 1]];
			std::cout << "However, the deadline of " << last_job.get_id() << " is " << last_job.get_deadline()
						<< std::endl;
		}
	}

	template<class Time> static bool print_feasibility_load_test_results(const Feasibility::Load_test<Time> &load_test) {
		if (load_test.is_certainly_infeasible()) {
			std::cout << "The given problem is infeasible. Assuming worst-case execution times and latest arrival times for all jobs, " <<
					"and maximum suspensions for all precedence constraints:" << std::endl;
			std::cout << " - at time " << load_test.get_current_time() << ", the processors must have spent at least ";
			std::cout << load_test.get_minimum_executed_load() << " time units executing jobs, but they can't possibly have spent more than ";
			std::cout << load_test.get_maximum_executed_load() << " time units executing jobs." << std::endl;
			return true;
		}
		std::cout << "The problem passed the necessary load-based feasibility test." << std::endl;
		return false;
	}

	template<class Time> static bool print_feasibility_interval_test_results(
		const Feasibility::Interval_test<Time> &interval_test, const Scheduling_problem<Time> &problem
	) {
		if (interval_test.is_certainly_infeasible()) {
			std::cout << "The given problem is infeasible. Assuming worst-case execution times and latest arrival times for all jobs, " <<
					"and maximum suspensions for all precedence constraints:" << std::endl;
			std::cout << " - between time " << interval_test.get_critical_start_time() << " and time " << interval_test.get_critical_end_time();
			std::cout << ", the processors must spent at least " << interval_test.get_critical_load() << " time units executing jobs, ";
			std::cout << "which requires more than " << problem.num_processors << " processors." << std::endl;
			return true;
		}
		std::cout << "The problem passed the necessary interval-based feasibility test." << std::endl;
		return false;
	}

	template<class Time> static void print_cuts(const std::vector<Rating_graph_cut> &cuts, const Scheduling_problem<Time> &problem) {
		std::cout << "There are " << cuts.size() << " cuts:" << std::endl;
		for (const auto &cut : cuts) {
			std::cout << "At node " << cut.node_index << ": ";
			if (!cut.allowed_jobs.empty()) {
				std::cout << "allow ";
				for (const auto job_index : cut.allowed_jobs) std::cout << problem.jobs[job_index].get_id() << ", ";
				std::cout << "and ";
			}
			std::cout << "forbid ";
			for (const auto job_index : cut.forbidden_jobs) std::cout << problem.jobs[job_index].get_id() << ", ";
			std::cout << "and safe jobs are ";
			for (const auto job_index : cut.safe_jobs) std::cout << problem.jobs[job_index].get_id() << ", ";
			std::cout << std::endl;
		}
	}

	template<class Time> static void print_fixing_changes(const Scheduling_problem<Time> &problem, const size_t num_original_constraints) {
		std::cout << "The given problem seems to be unschedulable (using our scheduler), but you can make it ";
		std::cout << "certainly schedulable by adding all the following dispatch ordering constraints:" << std::endl;
		for (size_t index = num_original_constraints; index < problem.prec.size(); index++) {
			const auto &prec = problem.prec[index];
			std::cout << " - ensure that " << problem.jobs[prec.get_fromIndex()].get_id();
			std::cout << " must be dispatched before " << problem.jobs[prec.get_toIndex()].get_id() << std::endl;
		}
	}
}

#endif
