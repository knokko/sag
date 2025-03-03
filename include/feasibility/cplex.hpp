#ifndef FEASIBILITY_CPLEX_H
#define FEASIBILITY_CPLEX_H

#include <chrono>
#include <iostream>
#include <regex>

#include "problem.hpp"
#include "simple_bounds.hpp"

namespace NP::Feasibility {
	static std::vector<Job_index> find_safe_job_ordering_with_cplex(
			const Scheduling_problem<dtime_t> &problem, const Simple_bounds<dtime_t> &simple_bounds
	) {
		const char *file_path = tmpnam(NULL);
		FILE *file = fopen(file_path, "w");
		if (!file) {
			std::cout << "Failed to write to file " << file_path << std::endl;
			return {};
		}

		fprintf(file, "using CP;\n\n");
		fprintf(file, "int numJobs = %lu;\n", problem.jobs.size());
		fprintf(file, "int numCores = %u;\n", problem.num_processors);
		fprintf(file, "range JobRange = 0..numJobs-1;\n");
		fprintf(file, "range CoreRange = 0..numCores-1;\n\n");

		fprintf(file, "tuple Job {\n\tint releaseTime;\n\tint executionTime;\n\tint deadline;\n};\n\n");
		fprintf(file, "Job jobs[JobRange] = [\n");
		for (const auto &job : problem.jobs) {
			const dtime_t start_time = simple_bounds.earliest_pessimistic_start_times[job.get_job_index()];
			const dtime_t end_time = simple_bounds.latest_safe_start_times[job.get_job_index()] + job.maximal_exec_time();
			if (end_time > 2147483647) throw std::runtime_error("Deadline is too late; the latest allowed deadline is 2^31-1 = 2147483647");
			fprintf(file, "\t<%llu, %llu, %llu>%s\n", start_time, job.maximal_exec_time(), end_time, job.get_job_index() == problem.jobs.size() - 1 ? "" : ",");
		}
		fprintf(file, "];\n\n");

		fprintf(file, "tuple PrecedenceConstraint {\n\tint beforeIndex;\n\tint afterIndex;\n\tint suspension;\n};\n\n");
		fprintf(file, "{ PrecedenceConstraint } precedenceConstraints = {\n");
		size_t pc_counter = 0;
		for (const auto &pc : problem.prec) {
			pc_counter += 1;
			fprintf(file, "\t<%lu, %lu, %llu>%s\n", pc.get_fromIndex(), pc.get_toIndex(), pc.get_maxsus(), pc_counter == problem.prec.size() ? "" : ",");
		}
		fprintf(file, "};\n\n");

		fprintf(file, "dvar int scheduledOn[JobRange];\n");
		fprintf(file, "dvar int scheduleTime[JobRange];\n");
		fprintf(file, "dvar interval core[CoreRange][JobRange] optional;\n");
		fprintf(file, "dvar sequence coreSequence[c in CoreRange] in all(jobIndex in JobRange) core[c][jobIndex];\n\n");

		fprintf(file, "subject to {\n");
		fprintf(file, "\tforall (jobIndex in JobRange) {\n");
		fprintf(file, "\t\tscheduledOn[jobIndex] >= 0 && scheduledOn[jobIndex] < numCores;\n");
		fprintf(file, "\t\tscheduleTime[jobIndex] >= jobs[jobIndex].releaseTime;\n");
		fprintf(file, "\t\tforall (c in CoreRange) {\n");
		fprintf(file, "\t\t\tpresenceOf(core[c][jobIndex]) == (scheduledOn[jobIndex] == c);\n");
		fprintf(file, "\t\t\tpresenceOf(core[c][jobIndex]) => (sizeOf(core[c][jobIndex]) == jobs[jobIndex].executionTime && startOf(core[c][jobIndex]) == scheduleTime[jobIndex] && endOf(core[c][jobIndex]) <= jobs[jobIndex].deadline);\n");
		fprintf(file, "\t\t}\n");
		fprintf(file, "\t}\n");
		fprintf(file, "\tforall (pc in precedenceConstraints) {\n");
		fprintf(file, "\t\tscheduleTime[pc.beforeIndex] + jobs[pc.beforeIndex].executionTime + pc.suspension <= scheduleTime[pc.afterIndex];\n");
		fprintf(file, "\t}\n");
		fprintf(file, "\tforall (c in CoreRange) noOverlap(coreSequence[c]);\n");
		fprintf(file, "}\n\n");

		fprintf(file, "execute {\n");
		fprintf(file, "\tfor (jobIndex in JobRange) writeln(\"dispatched job \" + jobIndex + \" at time \" + Opl.startOf(core[scheduledOn[jobIndex]][jobIndex]) + \" and core \" + scheduledOn[jobIndex]);\n");
		fprintf(file, "}\n");
		fflush(file);

		const auto start_time = std::chrono::high_resolution_clock::now();
		std::string command = "oplrun ";
		command.append(file_path);

#ifdef _WIN32
		auto opl = _popen(command.c_str(), "r");
#else
		auto opl = popen(command.c_str(), "r");
#endif
		if (opl == nullptr) {
			std::cout << "Failed to start opl" << std::endl;
			return {};
		}

		std::string opl_output_string;

		const int output_buffer_size = 100;
		char output_buffer[output_buffer_size];

		std::string output_string;

		while (fgets(output_buffer, output_buffer_size, opl) != nullptr) {
			output_string.append(output_buffer);
		}

#ifdef _WIN32
		int opl_status = _pclose(opl);
#else
		int opl_status = pclose(opl);
#endif
		const auto stop_time = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::ratio<1, 1>> spent_time = stop_time - start_time;
		std::cout << "oplrun needed " << spent_time.count() << " seconds" << std::endl;

		if (output_string.find("<<< no solution") != std::string::npos) {
			std::cout << "The problem is infeasible." << std::endl;
			return {};
		}

		if (opl_status != 0) {
			std::cout << "opl failed with status " << opl_status << " and output " << output_string << std::endl;
			return {};
		}

		if (output_string.find("OBJECTIVE: no objective") == std::string::npos) {
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

			std::string search_string = "dispatched job ";
			search_string.append(std::to_string(job_index));
			search_string.append(" at time ");

			size_t start_index = output_string.find(search_string);
			if (start_index == -1) {
				std::cout << "Couldn't find " << search_string << std::endl;
				return {};
			}

			start_index += search_string.length();
			size_t end_index = start_index;
			while (output_string[end_index] != ' ') end_index += 1;
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
