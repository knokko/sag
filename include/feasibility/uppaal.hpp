#ifndef FEASIBILITY_UPPAAL_H
#define FEASIBILITY_UPPAAL_H

#include <chrono>
#include <iostream>
#include <regex>

#include "node.hpp"
#include "problem.hpp"
#include "simple_bounds.hpp"

namespace NP::Feasibility {
	static void find_safe_job_ordering_with_uppaal(
		const Scheduling_problem<dtime_t> &problem, const Simple_bounds<dtime_t> &simple_bounds
	) {
		const auto predecessors = create_predecessor_mapping(problem);
		for (const auto &job : problem.jobs) {
			const size_t index = job.get_job_index();
			std::cout << "int job" << index << "_remaining_predecessors = " << predecessors[index].size() << ";" << std::endl;
			std::cout << "time job" << index << "_earliest_start_at = " << simple_bounds.earliest_pessimistic_start_times[index] << ";" << std::endl;
			std::cout << "time job" << index << "_execution_time = " << job.maximal_exec_time() << ";" << std::endl;
			std::cout << "time job" << index << "_latest_start_at = " << simple_bounds.latest_safe_start_times[index] << ";" << std::endl;
			std::cout << "time job" << index << "_started_at = -1;" << std::endl << std::endl;
		}

		for (const auto &job : problem.jobs) {
			const size_t index = job.get_job_index();
			std::cout << "Job" << index << " = Job(job" << index << "_remaining_predecessors, job" << index << "_earliest_start_at, job" << index;
			std::cout << "_execution_time, job" << index << "_latest_start_at, job" << index << "_started_at);" << std::endl;
			std::cout << "Failed" << index << " = FailedJob(job" << index << "_latest_start_at, job" << index << "_started_at);" << std::endl;
		}
		std::cout << std::endl;
		std::cout << "system";
		for (const auto &job : problem.jobs) std::cout << " Job" << job.get_job_index() << ",";
		std::cout << " < ";
		for (const auto &job : problem.jobs) std::cout << " Failed" << job.get_job_index() << " <";
		std::cout << std::endl;
	}
}

#endif
