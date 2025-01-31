#ifndef DUMP_PROBLEM_H
#define DUMP_PROBLEM_H

#include <iostream>

#include "problem.hpp"

namespace NP {
	static void dump_problem(
			const char *jobs_path, const char *prec_path, const Scheduling_problem<dtime_t> &problem
	) {
		FILE *jobs_file = fopen(jobs_path, "w");
		if (!jobs_file) {
			std::cout << "Failed to write to jobs file " << jobs_path << std::endl;
			return;
		}

		FILE *prec_file = fopen(prec_path, "w");
		if (!prec_file) {
			std::cout << "Failed to write to prec file " << prec_path << std::endl;
			return;
		}

		fprintf(jobs_file, "Task ID,Job ID,Arrival min,Arrival max,Cost min,Cost max,Deadline,Priority\n");
		for (const Job<dtime_t> &job : problem.jobs) {
			fprintf(jobs_file, "%lu,%lu,%lld,%lld,%lld,%lld,%lld,%lld\n", job.get_task_id(), job.get_job_id(),
					job.earliest_arrival(), job.latest_arrival(), job.least_exec_time(), job.maximal_exec_time(), job.get_deadline(), job.get_priority());
		}
		fflush(jobs_file);

		fprintf(prec_file, "Predecessor TID,	Predecessor JID,	Successor TID,	Successor JID,	Sus Min,	Sus Max,	Signal At\n");
		for (const Precedence_constraint<dtime_t> &prec : problem.prec) {
			fprintf(prec_file, "%lu,%lu, %lu,%lu, %lld,%lld, %s\n", prec.get_fromID().task, prec.get_fromID().job,
					prec.get_toID().task, prec.get_toID().job, prec.get_minsus(), prec.get_maxsus(), prec.should_signal_at_completion() ? "Completion" : "Start");
		}
		fflush(prec_file);
	}
}

#endif
