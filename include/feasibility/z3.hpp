#ifndef FEASIBILITY_Z3_H
#define FEASIBILITY_Z3_H

#include <chrono>
#include <regex>

#include "problem.hpp"
#include "simple_bounds.hpp"

namespace NP::Feasibility {
	static std::vector<Job_index> find_safe_job_ordering_with_z3(
			const Scheduling_problem<dtime_t> &problem, const Simple_bounds<dtime_t> &simple_bounds, int model, int timeout
	) {
		const auto generation_start_time = std::chrono::high_resolution_clock::now();
		const char *file_path = tmpnam(NULL);
		std::cout << "z3 model will be written to " << file_path << std::endl;
		FILE *file = fopen(file_path, "w");
		if (!file) {
			std::cout << "Failed to write to file " << file_path << std::endl;
			return {};
		}

		if (model == 1) {
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
					exit_when_timeout(timeout, generation_start_time, "generation for z3 timed out");
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
				dtime_t suspension = constraint.get_maxsus();
				if (constraint.should_signal_at_completion()) suspension += problem.jobs[constraint.get_fromIndex()].maximal_exec_time();
				fprintf(file, "(assert (<= (+ job%lu_start %llu) job%lu_start))\n", constraint.get_fromIndex(), suspension, constraint.get_toIndex());
			}

			
		} else if (model == 2) {
			// APPROACH 2
			// Variables
			// - global_jobD = J: job J is the Dth job that is dispatched                                      (source & enforced by constraint 1 & 2)
			// - global_coreD = C: the Dth job that is dispatched will be executed on core C                   (source & enforced by constraint 7)
			// - global_startD = T: the Dth job that is dispatched starts at time T                            (enforced by constraint 5, indirect 3)
			// - global_endD = T: the Dth job that is dispatched ends at time T                                (enforced by constraint 4)
			// - coreC_availableD = T: before the Dth job is dispatched, core C will be available at time T    (enforced by constraint 5 & 6)
			// - jobJ_start = T: job J starts executing at time T                                              (enforced by constraint 2 & 3 & 4)
			// - jobJ_dispatch = D: job J is the Dth job that is dispatched                                    (enforced by constraint 2 & 4)

			// Constraints
			// (0) for all dispatch indices D: global_jobD >= 0 and global_jobD < #jobs and global_coreD >= 0 && global_coreD < #cores
			// (1) for all jobs J and dispatch indices D: (jobJ_dispatch = D) => (global_jobD = J)
			// (2) for all jobs J: jobJ_start >= earliest_pessimistic_startJ and jobJ_start <= latest_safe_startJ and jobJ_dispatch >= 0 and jobJ_dispatch < #jobs
			// (3) for all precedence constraints from I to J: jobJ_start >= jobI_start + durationI + suspension
			// (4) for all jobs J and dispatch indices D: (global_jobD = J) => (jobJ_dispatch = D and jobJ_start = global_startD and global_endD = global_startD + durationJ)
			// (5) for all dispatch indices D and cores C: (global_coreD = C) => (global_startD >= coreC_availableD and coreC_available(D+1) = global_endD)
			// (6) for all cores C: coreC_available0 = 0
			// (7) for all dispatch indices D and cores C1, C2: (global_coreD = C1) => (coreC1_availableD <= coreC2_availableD)
			// (8) for all dispatch indices D and cores C: (global_coreD != C) => (coreC_available(D+1) = coreC_availableD)

			for (size_t index = 0; index < problem.jobs.size(); index++) {
				for (int core = 0; core < problem.num_processors; core++) {
					fprintf(file, "(declare-const core%u_available%lu Int)\n", core, index);
				}
				fprintf(file, "(declare-const global_job%lu Int)\n", index);
				fprintf(file, "(declare-const global_core%lu Int)\n", index);
				fprintf(file, "(declare-const global_start%lu Int)\n", index);
				fprintf(file, "(declare-const global_end%lu Int)\n\n", index);
			}

			for (const auto &job : problem.jobs) {
				if (job.get_min_parallelism() > 1) throw std::runtime_error("Multi-core jobs are not supported");
				fprintf(file, "(declare-const job%lu_start Int)\n", job.get_job_index());
				fprintf(file, "(declare-const job%lu_dispatch Int)\n", job.get_job_index());
			}

			for (size_t index = 0; index < problem.jobs.size(); index++) {
				// Constraints (0)
				fprintf(file, "(assert (>= global_job%lu 0))\n", index);
				fprintf(file, "(assert (< global_job%lu %lu))\n", index, problem.jobs.size());
				fprintf(file, "(assert (>= global_core%lu 0))\n", index);
				fprintf(file, "(assert (< global_core%lu %u))\n\n", index, problem.num_processors);

				for (int core = 0; core < problem.num_processors; core++) {
					if (index < problem.jobs.size() - 1) {
						// Constraints (5) case 1
						fprintf(
							file, "(assert (=> (= global_core%lu %u) (and (>= global_start%lu core%u_available%lu) (= core%u_available%lu global_end%lu))))\n",
							index, core, index, core, index, core, index + 1, index
						);

						// Constraints (8)
						fprintf(
							file, "(assert (=> (distinct global_core%lu %u) (= core%u_available%lu core%u_available%lu)))\n",
							index, core, core, index, core, index + 1
						);
					} else {
						// Constraints (5) case 2
						fprintf(
							file, "(assert (=> (= global_core%lu %u) (>= global_start%lu core%u_available%lu)))\n",
							index, core, index, core, index
						);
					}

					// Constraints (7)
					for (int other_core = 0; other_core < problem.num_processors; other_core++) {
						if (core == other_core) continue;
						fprintf(
							file, "(assert (=> (= global_core%lu %u) (<= core%u_available%lu core%u_available%lu)))\n",
							index, core, core, index, other_core, index
						);
					}
				}
			}

			for (const auto &job : problem.jobs) {
				// Constraints (2)
				fprintf(
						file, "(assert (>= job%lu_start %llu))\n", job.get_job_index(),
						simple_bounds.earliest_pessimistic_start_times[job.get_job_index()]
				);
				fprintf(
						file, "(assert (<= job%lu_start %llu))\n\n",
						job.get_job_index(), simple_bounds.latest_safe_start_times[job.get_job_index()]
				);
				fprintf(file, "(assert (>= job%lu_dispatch 0))\n", job.get_job_index());
				fprintf(file, "(assert (< job%lu_dispatch %lu))\n\n", job.get_job_index(), problem.jobs.size());
			}

			// Constraints (3)
			for (const auto &constraint : problem.prec) {
				dtime_t suspension = constraint.get_maxsus();
				if (constraint.should_signal_at_completion()) suspension += problem.jobs[constraint.get_fromIndex()].maximal_exec_time();
				fprintf(file, "(assert (<= (+ job%lu_start %llu) job%lu_start))\n", constraint.get_fromIndex(), suspension, constraint.get_toIndex());
			}

			// Constraints (6)
			for (int core = 0; core < problem.num_processors; core++) {
				fprintf(file, "(assert (= core%u_available0 0))\n", core);
			}

			for (size_t index = 0; index < problem.jobs.size(); index++) {
				for (const auto &job : problem.jobs) {
					// Constraints (1)
					fprintf(file, "(assert (=> (= job%lu_dispatch %lu) (= global_job%lu %lu)))\n", job.get_job_index(), index, index, job.get_job_index());

					// Constraints (4)
					fprintf(
						file, "(assert (=> (= global_job%lu %lu) (and (= job%lu_dispatch %lu) (= job%lu_start global_start%lu) (= global_end%lu (+ global_start%lu %llu)))))\n",
						index, job.get_job_index(), job.get_job_index(), index, job.get_job_index(), index, index, index, job.maximal_exec_time()
					);
					exit_when_timeout(timeout, generation_start_time, "generation for z3 timed out");
				}
			}
		} else throw std::runtime_error("Unknown z3 model");

		fprintf(file, "(check-sat)\n");
		fprintf(file, "(get-model)\n");
		fflush(file);

		const auto start_time = std::chrono::high_resolution_clock::now();

		const auto z3_path = getenv("Z3_PATH");
		std::string command = "";
		if (z3_path) command.append(z3_path);
		else command.append("z3");

		command.append(" ");

		if (timeout != 0) {
			command.append("-T:");
			command.append(std::to_string(timeout));
			command.append(" ");
		}
		command.append(file_path);

#ifdef _WIN32
		auto z3 = _popen(command.c_str(), "r");
#else
		auto z3 = popen(command.c_str(), "r");
#endif
		if (z3 == nullptr) {
			std::cout << "Failed to start z3" << std::endl;
			return {};
		}

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

		if (output_string.starts_with("timeout")) {
			std::cout << "z3 timed out" << std::endl;
			exit(0);
		}

		const auto stop_time = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::ratio<1, 1>> spent_time = stop_time - start_time;
		std::cout << "z3 needed " << spent_time.count() << " seconds" << std::endl;

		if (output_string.starts_with("unsat")) {
			std::cout << "The problem is infeasible." << std::endl;
			return {};
		}

		if (z3_status != 0 && z3_status != 256) {
			std::cout << "z3 failed with status " << z3_status << " and output " << output_string << std::endl;
			if (z3_status == 32512) std::cout << "Use the Z3_PATH environment variable to specify the location of the z3 executable" << std::endl;
			return {};
		}

		if (!output_string.starts_with("sat")) {
			std::cout << "Unexpected output: " << output_string << std::endl;
			return {};
		}

		output_string = std::regex_replace(output_string, std::regex("\n"), "");
		output_string = std::regex_replace(output_string, std::regex("\r"), "");

		struct Start_time {
			Job_index job_index;
			dtime_t start_time;
		};
		std::vector<Start_time> start_times;
		start_times.reserve(problem.jobs.size());

		for (Job_index job_index = 0; job_index < problem.jobs.size(); job_index++) {
			const auto &job = problem.jobs[job_index];

			std::string search_string = "define-fun job";
			search_string.append(std::to_string(job_index));
			search_string.append("_start () Int    ");

			size_t start_index = output_string.find(search_string);
			if (start_index == -1) {
				std::cout << "Couldn't find " << search_string << std::endl;
				return {};
			}

			start_index += search_string.length();
			size_t end_index = start_index;
			while (output_string[end_index] != ')') end_index += 1;
			dtime_t start_time = std::stoll(output_string.substr(start_index, end_index));
			start_times.push_back(Start_time { .job_index=job_index, .start_time=start_time });
		}

		std::sort(start_times.begin(), start_times.end(), [](const Start_time &a, const Start_time &b) {
			return a.start_time < b.start_time;
		});

		// TODO Respect dispatch ordering constraints when jobs are dispatched at the same time
		std::vector<Job_index> result(start_times.size(), 0);
		for (size_t index = 0; index < result.size(); index++) result[index] = start_times[index].job_index;
		return result;
	}
}

#endif
