#ifndef FEASIBILITY_Z3_H
#define FEASIBILITY_Z3_H

#include <regex>

#include "problem.hpp"
#include "simple_bounds.hpp"

namespace NP::Feasibility {
	template<class Time> static void generate_z3(
			const char *file_path, const Scheduling_problem<Time> &problem, const SimpleBounds<Time> &simple_bounds
	) {
		FILE *file = fopen(file_path, "w");
		if (!file) {
			std::cout << "Failed to write to file " << file_path << std::endl;
			return;
		}

		// APPROACH 1
		// Variables
		// - jobJ_start = T: job J starts executing at time T
		// - jobJ_core = C: job J is executed on core C

		// Constraints
		// (1) for all jobs J: 0 <= jobJ_core < num_cores
		// (2) for all jobs J: jobJ_start >= earliest_pessimistic_startJ and jobJ_start <= latest_safe_startJ
		// (3) for all jobs I, J: (jobI_core = jobJ_core and jobI_start <= jobJ_start) => (jobJ_start >= jobI_start + durationI)
		// (4) for all precedence constraints from I to J: jobJ_start >= jobI_start + durationI + suspension

		// constraint (3) can be skipped when earliest_start[I] >= latest_start[J] + durationJ or earliest_start[J] >= latest_start[I] + durationI

		for (const auto &job : problem.jobs) {
			if (job.get_min_parallelism() > 1) throw std::runtime_error("Multi-core jobs are not supported");

			fprintf(file, "(declare-const job%lu_start Int)\n", job.get_job_index());
			fprintf(file, "(declare-const job%lu_core Int)\n", job.get_job_index());

			// Constraints (1)
			fprintf(file, "(assert (>= job%lu_core 0))\n", job.get_job_index());
			fprintf(file, "(assert (< job%lu_core %u))\n\n", job.get_job_index(), problem.num_processors);

			// Constraints (2)
			fprintf(
					file, "(assert (>= job%lu_start %llu))\n", job.get_job_index(),
					simple_bounds.earliest_pessimistic_start_times[job.get_job_index()]
			);
			fprintf(
					file, "(assert (<= job%lu_start %llu))\n\n",
					job.get_job_index(), simple_bounds.latest_safe_start_times[job.get_job_index()]
			);
		}

		// Constraints (3) + optimization
		for (Job_index first_job_index = 0; first_job_index < problem.jobs.size(); first_job_index++) {
			for (Job_index later_job_index = 0; later_job_index < problem.jobs.size(); later_job_index++) {
				if (first_job_index == later_job_index) continue;

				if (simple_bounds.latest_safe_start_times[later_job_index] < simple_bounds.earliest_pessimistic_start_times[first_job_index]) {
					// Ignore pairs of jobs where the 'later' job must start before the 'first' job
					continue;
				}

				if (simple_bounds.latest_safe_start_times[first_job_index] + problem.jobs[first_job_index].maximal_exec_time() <= simple_bounds.earliest_pessimistic_start_times[later_job_index]) {
					// Ignore pairs of jobs where the 'later' job can't start before the 'first' job is completed
					continue;
				}

				fprintf(
						file, "(assert (=> (and (= job%lu_core job%lu_core) (<= job%lu_start job%lu_start)) "
							  "(>= job%lu_start (+ job%lu_start %llu))))\n",
						first_job_index, later_job_index, first_job_index, later_job_index,
						later_job_index, first_job_index, problem.jobs[first_job_index].maximal_exec_time()
				);
			}
		}

		for (const auto &constraint : problem.prec) {
			// Constraints (4)
			fprintf(
					file, "(assert (>= (+ job%lu_start %llu) job%lu_start))\n", constraint.get_fromIndex(),
					problem.jobs[constraint.get_fromIndex()].maximal_exec_time() + constraint.get_suspension().max(),
					constraint.get_toIndex()
			);
		}

		fprintf(file, "(check-sat)\n");
		fprintf(file, "(get-model)\n");
		fflush(file);

		std::string command = "z3 ";
		command.append(file_path);

#ifdef _WIN32
		auto z3 = _popen(command.c_str(), "r");
#else
		auto z3 = popen(command.c_str(), "r");
#endif
		if (z3 == nullptr) {
			std::cout << "Failed to start z3" << std::endl;
			return;
		}

		std::string z3_output_string;

		const int output_buffer_size = 100;
		char output_buffer[output_buffer_size];

		std::string output_string;

		while (fgets(output_buffer, output_buffer_size, z3) != nullptr) {
			output_string.append(output_buffer);
		}

#ifdef _WIN32
		int z3_status = _pclose(z3);
#else
		int z3_status = pclose(z3);
#endif
		if (z3_status != 0) {
			std::cout << "z3 failed with status " << z3_status << " and output " << output_string << std::endl;
			return;
		}

		if (output_string.starts_with("unsat")) {
			std::cout << "The problem is infeasible." << std::endl;
			return;
		}

		if (!output_string.starts_with("sat")) {
			std::cout << "Unexpected output: " << output_string << std::endl;
			return;
		}

		output_string = std::regex_replace(output_string, std::regex("\n"), "");
		output_string = std::regex_replace(output_string, std::regex("\r"), "");

		struct Start_time {
			Job_index job_index;
			Time start_time;
		};
		std::vector<Start_time> start_times;
		start_times.reserve(problem.jobs.size());

		std::cout << "The problem is feasible. I will present a correct schedule:" << std::endl;
		for (Job_index job_index = 0; job_index < problem.jobs.size(); job_index++) {
			const auto &job = problem.jobs[job_index];

			std::string search_string = "define-fun job";
			search_string.append(std::to_string(job_index));
			search_string.append("_start () Int    ");

			size_t start_index = output_string.find(search_string);
			if (start_index == -1) {
				std::cout << "Couldn't find " << search_string << std::endl;
				return;
			}

			start_index += search_string.length();
			size_t end_index = start_index;
			while (output_string[end_index] != ')') end_index += 1;
			Time start_time = std::stoll(output_string.substr(start_index, end_index));
			start_times.push_back(Start_time { .job_index=job_index, .start_time=start_time });
		}

		std::sort(start_times.begin(), start_times.end(), [](const Start_time &a, const Start_time &b) {
			return a.start_time < b.start_time;
		});

		for (const auto &st : start_times) {
			const auto &job = problem.jobs[st.job_index];
			std::cout << "Job " << job.get_id() << " should start at " << st.start_time << " and end at " <<
					(st.start_time + job.maximal_exec_time()) << " (deadline is " << job.get_deadline() << ")" << std::endl;
		}
	}
}

#endif
