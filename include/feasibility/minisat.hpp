#ifndef FEASIBILITY_MINISAT_H
#define FEASIBILITY_MINISAT_H

#include <iostream>
#include <regex>

#include "problem.hpp"
#include "simple_bounds.hpp"
#include "timeout.hpp"

namespace NP::Feasibility {
	static std::vector<Job_index> find_safe_job_ordering_with_minisat(
			const Scheduling_problem<dtime_t> &problem, const Simple_bounds<dtime_t> &simple_bounds, double timeout
	) {
		const auto generation_start_time = std::chrono::high_resolution_clock::now();
		if (!problem.prec.empty()) throw std::runtime_error("The minisat model doesn't support precedence constraints");
		const char *input_file_path = tmpnam(NULL);
		FILE *file = fopen(input_file_path, "w");
		if (!file) {
			std::cout << "Failed to write to file " << input_file_path << std::endl;
			return {};
		}
		std::cout << "minisat model will be written to file " << input_file_path << std::endl;

		dtime_t bound_time = 0;
		for (const auto &job : problem.jobs) {
			bound_time = std::max(bound_time, job.get_deadline() - 1);
		}

		size_t variable_index = 1;
		std::vector<size_t> variables_u(problem.jobs.size(), 0);
		for (const auto &job : problem.jobs) {
			variables_u[job.get_job_index()] = variable_index;
			variable_index += problem.num_processors;
		}

		std::vector<size_t> variables_z(problem.jobs.size(), 0);
		for (const auto &job : problem.jobs) {
			variables_z[job.get_job_index()] = variable_index;
			variable_index += simple_bounds.latest_safe_start_times[job.get_job_index()] + job.maximal_exec_time() - simple_bounds.earliest_pessimistic_start_times[job.get_job_index()];
		}

		std::vector<size_t> variables_d(problem.jobs.size());
		for (const auto &job : problem.jobs) {
			variables_d[job.get_job_index()] = variable_index;
			variable_index += problem.num_processors * (1 + simple_bounds.latest_safe_start_times[job.get_job_index()] - simple_bounds.earliest_pessimistic_start_times[job.get_job_index()]);
		}

		// Clauses D1
		for (const auto &job : problem.jobs) {
			for (int high_processor = 0; high_processor < problem.num_processors; high_processor++) {
				const size_t high_variable = variables_u[job.get_job_index()] + high_processor;
				for (int low_processor = 0; low_processor < high_processor; low_processor++) {
					const size_t low_variable = variables_u[job.get_job_index()] + low_processor;
					fprintf(file, "-%lu -%lu 0\n", low_variable, high_variable);
				}
			}
		}

		// Clauses D2
		for (const auto &job : problem.jobs) {
			for (int processor = 0; processor < problem.num_processors; processor++) {
				fprintf(file, "%lu ", variables_u[job.get_job_index()] + processor);
			}
			fprintf(file, "0\n");
		}

		// Clauses D3
		for (size_t high_index = 0; high_index < problem.jobs.size(); high_index++) {
			const auto &high_job = problem.jobs[high_index];
			for (size_t low_index = 0; low_index < high_index; low_index++) {
				const auto &low_job = problem.jobs[low_index];
				const dtime_t start_time = std::max(simple_bounds.earliest_pessimistic_start_times[high_index], simple_bounds.earliest_pessimistic_start_times[low_index]);
				const dtime_t bound_time = std::min(
					simple_bounds.latest_safe_start_times[high_index] + high_job.maximal_exec_time(),
					simple_bounds.latest_safe_start_times[low_index] + low_job.maximal_exec_time()
				);

				// Skip clause when the jobs can't overlap
				if (start_time >= bound_time) continue;

				for (dtime_t time = 0; time < bound_time - start_time; time++) {
					for (int processor = 0; processor < problem.num_processors; processor++) {
						const size_t variable_u_high = variables_u[high_index] + processor;
						const size_t variable_u_low = variables_u[low_index] + processor;
						const size_t variable_z_high = variables_z[high_index] + time + start_time - simple_bounds.earliest_pessimistic_start_times[high_index];
						const size_t variable_z_low = variables_z[low_index] + time + start_time - simple_bounds.earliest_pessimistic_start_times[low_index];
						fprintf(file, "-%lu -%lu -%lu -%lu 0\n", variable_z_low, variable_u_low, variable_z_high, variable_u_high);
					}

					exit_when_timeout(timeout, generation_start_time, "generation for minisat timed out"); // TODO Use this more
				}
			}
		}

		// Clauses D4
		for (const auto &job : problem.jobs) {
			for (dtime_t relative = 0; relative < problem.num_processors * (1 + simple_bounds.latest_safe_start_times[job.get_job_index()] - simple_bounds.earliest_pessimistic_start_times[job.get_job_index()]); relative++) {
				fprintf(file, "%llu ", variables_d[job.get_job_index()] + relative);
				exit_when_timeout(timeout, generation_start_time, "generation for minisat timed out");
			}
			fprintf(file, "0\n");
		}

		// Clauses D5
		for (const auto &job : problem.jobs) {
			const dtime_t num_start_times = 1 + simple_bounds.latest_safe_start_times[job.get_job_index()] - simple_bounds.earliest_pessimistic_start_times[job.get_job_index()];
			for (int processor = 0; processor < problem.num_processors; processor++) {
				for (dtime_t relative_time = 0; relative_time < num_start_times; relative_time++) {
					const size_t variable_u = variables_u[job.get_job_index()] + processor;
					const size_t variable_d = variables_d[job.get_job_index()] + processor * num_start_times + relative_time;
					fprintf(file, "-%lu %lu 0\n", variable_d, variable_u);

					for (dtime_t running_time = 0; running_time < job.maximal_exec_time(); running_time++) {
						const size_t variable_z = variables_z[job.get_job_index()] + relative_time + running_time;
						fprintf(file, "-%lu %lu 0\n", variable_d, variable_z);
						exit_when_timeout(timeout, generation_start_time, "generation for minisat timed out");
					}

					fprintf(file, "-%lu %lu ", variable_u, variable_d);
					for (dtime_t running_time = 0; running_time < job.maximal_exec_time(); running_time++) {
						const size_t variable_z = variables_z[job.get_job_index()] + relative_time + running_time;
						fprintf(file, "-%lu ", variable_z);
						exit_when_timeout(timeout, generation_start_time, "generation for minisat timed out");
					}
					fprintf(file, "0\n");
				}
			}
			fprintf(file, "\n");
		}
		fflush(file);

		const auto start_time = std::chrono::high_resolution_clock::now();

		const auto minisat_path = getenv("MINISAT_PATH");
		std::string command = "";

		if (timeout != 0.0) {
			command.append("timeout ");
			command.append(std::to_string(timeout));
			command.append("s ");
		}

		if (minisat_path) command.append(minisat_path);
		else command.append("minisat");

		command.append(" ");
		command.append(input_file_path);
		command.append(" ");

		const char *output_file_path = tmpnam(NULL);
		std::cout << "minisat output will be written to " << output_file_path << std::endl;
		command.append(output_file_path);
		command.append(" 2>/dev/null");

#ifdef _WIN32
		auto minisat = _popen(command.c_str(), "r");
#else
		auto minisat = popen(command.c_str(), "r");
#endif
		if (minisat == nullptr) {
			std::cout << "Failed to start minisat" << std::endl;
			return {};
		}

		std::string std_output_string;

		const int output_buffer_size = 100;
		char output_buffer[output_buffer_size];

		while (fgets(output_buffer, output_buffer_size, minisat) != nullptr) {
			std_output_string.append(output_buffer);
		}

#ifdef _WIN32
		int minisat_status = _pclose(minisat);
#else
		int minisat_status = pclose(minisat);
#endif

		if (minisat_status == 31744) {
			std::cout << "minisat timed out" << std::endl;
			return {};
		}

		if (minisat_status == 5120) {
			std::cout << "The problem is infeasible." << std::endl;
			return {};
		}

		if (minisat_status != 2560) {
			std::cout << "minisat failed with status " << minisat_status << " and output " << std_output_string << std::endl;
			if (minisat_status == 32512) std::cout << "Use the MINISAT_PATH environment variable to specify the location of the minisat executable" << std::endl;
			return {};
		}

		const auto stop_time = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::ratio<1, 1>> spent_time = stop_time - start_time;
		std::cout << "minisat needed " << spent_time.count() << " seconds" << std::endl;

		std::string output_string;
		FILE *output_file = fopen(output_file_path, "r");
		if (!output_file) {
			std::cout << "Failed to read minisat output file " << output_file_path << std::endl;
			return {};
		}
		while (fgets(output_buffer, output_buffer_size, output_file) != nullptr) {
			output_string.append(output_buffer);
		}
		output_string = std::regex_replace(output_string, std::regex("\n"), "");
		output_string = std::regex_replace(output_string, std::regex("\r"), "");
		if (!output_string.starts_with("SAT")) {
			std::cout << "Unexpected output file content: " << output_string << std::endl;
			return {};
		}

		struct Start_time {
			Job_index job_index;
			dtime_t start_time;
		};
		std::vector<Start_time> start_times;
		start_times.reserve(problem.jobs.size());

		size_t start_index = 3;
		size_t job_index = 0;
		while (start_index < output_string.size()) {
			size_t bound_index = start_index;
			while (bound_index < output_string.size() && output_string[bound_index] != ' ') bound_index += 1;
			long next_int = std::stol(output_string.substr(start_index, bound_index));
			size_t positive_int = std::abs(next_int);
			if (next_int > 0 && positive_int >= variables_d[job_index]) {
				const dtime_t num_start_times = 1 + simple_bounds.latest_safe_start_times[job_index] - simple_bounds.earliest_pessimistic_start_times[job_index];
				const size_t relative_index = positive_int - variables_d[job_index];
				const dtime_t start_time = (simple_bounds.earliest_pessimistic_start_times[job_index] + relative_index % num_start_times);
				start_times.push_back({ Start_time { .job_index=job_index, .start_time=start_time }});
				job_index += 1;
			}
			start_index = bound_index + 1;
		}

		std::sort(start_times.begin(), start_times.end(), [](const Start_time &a, const Start_time &b) {
			return a.start_time < b.start_time;
		});

		std::vector<Job_index> result(start_times.size(), 0);
		for (size_t index = 0; index < result.size(); index++) result[index] = start_times[index].job_index;
		return result;
	}
}

#endif
