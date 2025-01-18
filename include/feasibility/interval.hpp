#ifndef FEASIBILITY_INTERVAL_H
#define FEASIBILITY_INTERVAL_H

#include "problem.hpp"

#include "simple_bounds.hpp"
#include "interval_tree.hpp"

#include <iostream>

namespace NP::Feasibility {
	template<class Time> class Interval_test {
		const Scheduling_problem<Time> &problem;
		const Simple_bounds<Time> &simple_bounds;
		Interval_tree<Time> interval_tree;

		Job_index next_job_index = 0;
		bool certainly_infeasible = false;

		std::vector<Finterval<Time>> relevant_jobs;
		Time start_time;
		Time end_time;
		Time required_load;
	public:
		Interval_test(const Scheduling_problem<Time> &problem, const Simple_bounds<Time> &simple_bounds) : problem(problem), simple_bounds(simple_bounds) {
			for (Job_index job_index = 0; job_index < problem.jobs.size(); job_index++) {
				interval_tree.insert(Finterval<Time> {
					.job_index=job_index,
					.start=simple_bounds.earliest_pessimistic_start_times[job_index],
					.end=simple_bounds.latest_safe_start_times[job_index] + problem.jobs[job_index].maximal_exec_time()
				});
			}
			interval_tree.split();
		}

		bool next() {
			if (certainly_infeasible || next_job_index >= problem.jobs.size()) return false;

			required_load = 0;
			start_time = simple_bounds.earliest_pessimistic_start_times[next_job_index];
			end_time = simple_bounds.latest_safe_start_times[next_job_index] + problem.jobs[next_job_index].maximal_exec_time();

			// Find all jobs that satisfy both conditions:
			// - their latest safe start time is smaller than end_time
			// - their earliest possible finish time is larger than start_time
			interval_tree.query(Finterval<Time> { .job_index=next_job_index, .start=start_time, .end=end_time }, relevant_jobs);

			for (const auto &interval : relevant_jobs) {
				Time non_overlapping_time = 0;
				if (interval.start < start_time) non_overlapping_time = start_time - interval.start;
				if (interval.end > end_time) non_overlapping_time = std::max(non_overlapping_time, interval.end - end_time);
				Time exec_time = problem.jobs[interval.job_index].maximal_exec_time();
				if (exec_time > non_overlapping_time) required_load += exec_time - non_overlapping_time;
			}

			if (required_load > (end_time - start_time) * problem.num_processors) certainly_infeasible = true;
			relevant_jobs.clear();
			next_job_index += 1;
			return true;
		}

		bool is_certainly_infeasible() const {
			return certainly_infeasible;
		}

		Time get_critical_start_time() const {
			return start_time;
		}

		Time get_critical_end_time() const {
			return end_time;
		}

		Time get_critical_load() const {
			return required_load;
		}
	};
}

#endif
