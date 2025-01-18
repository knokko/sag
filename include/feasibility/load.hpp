#ifndef FEASIBILITY_LOAD_H
#define FEASIBILITY_LOAD_H

#include "problem.hpp"

#include "simple_bounds.hpp"

#include <iostream>

namespace NP::Feasibility {
	template<class Time> struct Load_job {
		Job_index job_index;
		/**
		 * An upper bound on the time until this job is finished
		 */
		Time maximum_remaining_time;

		Time get_minimum_spent_time(const Scheduling_problem<Time> &problem) const {
			Time exec_time = problem.jobs[job_index].maximal_exec_time();
			std::cout << "exec time is " << exec_time << " and max remaining is " << maximum_remaining_time << std::endl;
			assert(exec_time >= maximum_remaining_time);
			return exec_time - maximum_remaining_time;
		}
	};

	template<class Time> class Load_test {
		const Scheduling_problem<Time> &problem;
		const Simple_bounds<Time> &simple_bounds;
		std::vector<Job_index> jobs_by_earliest_start_time;
		std::vector<Job_index> jobs_by_latest_safe_start_time;
		std::vector<Time> times_of_interest;

		/**
		 * The current simulation/test time
		 */
		Time current_time = 0;

		size_t time_index = 0;
		Job_index next_early_job_index = 0;
		Job_index next_late_job_index = 0;

		bool certainly_infeasible = false;

		/**
		 * The sum of the worst-case execution times of all jobs that must have finished already.
		 */
		Time certainly_finished_jobs_load = 0;

		/**
		 * A lower bound on the total time that the processors must have spent executing jobs, assuming
		 * worst-case arrival times and execution times for all jobs.
		 * When this becomes larger than maximum_executed_load, the problem is certainly infeasible.
		 */
		Time minimum_executed_load = 0;
		/**
		 * An upper bound on the total time that the processors could have spent executing jobs.
		 * This is definitely not larger than num_processors * current_time, but could be lower.
		 */
		Time maximum_executed_load = 0;
		/**
		 * The list/set of jobs that could be running right now, assuming worst-case arrival times
		 * and execution times for all jobs.
		 */
		std::vector<Load_job<Time>> possibly_running_jobs;

		/**
		 * The list/set of jobs that must have started already, and may or may not have finished already,
		 * assuming worst-case arrival times and execution times of all jobs.
		 */
		std::vector<Load_job<Time>> certainly_started_jobs;
	public:
		Load_test(const Scheduling_problem<Time> &problem, const Simple_bounds<Time> &simple_bounds) : problem(problem), simple_bounds(simple_bounds) {
			jobs_by_earliest_start_time.reserve(problem.jobs.size());
			jobs_by_latest_safe_start_time.reserve(problem.jobs.size());
			times_of_interest.reserve(2 * problem.jobs.size());
			for (Job_index index = 0; index < problem.jobs.size(); index++) {
				jobs_by_earliest_start_time.push_back(index);
				jobs_by_latest_safe_start_time.push_back(index);
			}
			std::sort(jobs_by_earliest_start_time.begin(), jobs_by_earliest_start_time.end(), [simple_bounds, problem](const Job_index &a, const Job_index &b) {
				return simple_bounds.earliest_pessimistic_start_times[a] < simple_bounds.earliest_pessimistic_start_times[b];
			});
			std::sort(jobs_by_latest_safe_start_time.begin(), jobs_by_latest_safe_start_time.end(), [simple_bounds, problem](const Job_index &a, const Job_index &b) {
				return simple_bounds.latest_safe_start_times[a] < simple_bounds.latest_safe_start_times[b];
			});
			for (Job_index job_index = 0; job_index < problem.jobs.size(); job_index++) {
				Time start = simple_bounds.latest_safe_start_times[job_index];
				times_of_interest.push_back(start);
				times_of_interest.push_back(start + problem.jobs[job_index].maximal_exec_time());
			}
			std::sort(times_of_interest.begin(), times_of_interest.end());
		}

		bool next() {
			std::cout << "start iteration" << std::endl;
			while (time_index < times_of_interest.size() && times_of_interest[time_index] == current_time) time_index += 1;
			if (certainly_infeasible || time_index >= times_of_interest.size()) return false;
			Time next_time = times_of_interest[time_index];
			Time spent_time = next_time - current_time;
			
			Time earliest_step_arrival = next_time;
			for (const auto &running_job : possibly_running_jobs) {
				earliest_step_arrival = std::min(earliest_step_arrival, simple_bounds.earliest_pessimistic_start_times[running_job.job_index]);
			}

			Time maximum_load_this_step = 0;

			std::vector<Load_job<Time>> next_jobs;
			for (const auto &running_job : possibly_running_jobs) {
				if (running_job.maximum_remaining_time > spent_time) {
					maximum_load_this_step += spent_time;
					next_jobs.push_back(Load_job<Time> {
						.job_index=running_job.job_index,
						.maximum_remaining_time=running_job.maximum_remaining_time - spent_time
					});
				} else {
					certainly_finished_jobs_load += problem.jobs[running_job.job_index].maximal_exec_time();
					maximum_load_this_step += running_job.maximum_remaining_time;
					std::cout << "(2) increased certainly finished jobs load by " << problem.jobs[running_job.job_index].maximal_exec_time() << std::endl;
				}
			}
			possibly_running_jobs = next_jobs;

			while (next_early_job_index < problem.jobs.size() && simple_bounds.earliest_pessimistic_start_times[jobs_by_earliest_start_time[next_early_job_index]] <= next_time) {
				Job_index job_index = jobs_by_earliest_start_time[next_early_job_index];
				Time exec_time = problem.jobs[job_index].maximal_exec_time();
				Time latest_finish_time = simple_bounds.latest_safe_start_times[job_index] + exec_time;
				if (latest_finish_time > next_time) {
					Time maximum_remaining_time = latest_finish_time - next_time;
					possibly_running_jobs.push_back(Load_job<Time> {
						.job_index=job_index,
						.maximum_remaining_time=maximum_remaining_time
					});
					maximum_load_this_step += std::min(exec_time, next_time - simple_bounds.earliest_pessimistic_start_times[job_index]);
				} else {
					certainly_finished_jobs_load += exec_time;
					std::cout << "(1) increased certainly finished jobs load by " << exec_time << std::endl;
					maximum_load_this_step += exec_time;
				}
				
				next_early_job_index += 1;
			}

			next_jobs.clear();
			for (const auto &started_job : certainly_started_jobs) {
				std::cout << "max remaining time is " << started_job.maximum_remaining_time << " and spent time is " << spent_time << std::endl;
				if (started_job.maximum_remaining_time > spent_time) {
					next_jobs.push_back(Load_job<Time> {
						.job_index=started_job.job_index,
						.maximum_remaining_time=started_job.maximum_remaining_time - spent_time
					});
				}
			}
			certainly_started_jobs = next_jobs;

			while (next_late_job_index < problem.jobs.size() && simple_bounds.latest_safe_start_times[jobs_by_latest_safe_start_time[next_late_job_index]] <= next_time) {
				Job_index job_index = jobs_by_latest_safe_start_time[next_late_job_index];
				Time exec_time = problem.jobs[job_index].maximal_exec_time();
				Time latest_finish_time = simple_bounds.latest_safe_start_times[job_index] + exec_time;
				if (latest_finish_time > next_time) {
					certainly_started_jobs.push_back(Load_job<Time> {
						.job_index=job_index,
						.maximum_remaining_time=latest_finish_time - next_time
					});
					std::cout << "latest finish time is " << latest_finish_time << " and next time is " << next_time << std::endl;
				}
				next_late_job_index += 1;
			}

			// Minimize (sum worst_case_exec_time() of finished jobs) + (sum minimum_spent_time() of unfinished jobs)
			std::sort(certainly_started_jobs.begin(), certainly_started_jobs.end(), [](const Load_job<Time> &a, const Load_job<Time> &b) {
				return a.maximum_remaining_time < b.maximum_remaining_time;
			});
			minimum_executed_load = certainly_finished_jobs_load;
			size_t start_index = 0;
			std::cout << "minimum executed load initialized to " << minimum_executed_load << std::endl;
			// Since all these jobs must have started already, at least num_started_jobs - num_cores must have finished already
			if (problem.num_processors < certainly_started_jobs.size()) {
				for (; start_index < certainly_started_jobs.size() - problem.num_processors; start_index++) {
					minimum_executed_load += problem.jobs[certainly_started_jobs[start_index].job_index].maximal_exec_time();
					std::cout << "(1) increased by " << problem.jobs[certainly_started_jobs[start_index].job_index].maximal_exec_time() << " for job " << certainly_started_jobs[start_index].job_index << std::endl;
				}
			}
			for (; start_index < certainly_started_jobs.size(); start_index++) {
				minimum_executed_load += certainly_started_jobs[start_index].get_minimum_spent_time(problem);
				std::cout << "(2) increased by " << certainly_started_jobs[start_index].get_minimum_spent_time(problem) << std::endl;
			}

			Time max_load_bound2 = certainly_finished_jobs_load;
			for (const auto &running_job : possibly_running_jobs) {
				max_load_bound2 += problem.jobs[running_job.job_index].maximal_exec_time();
				earliest_step_arrival = std::min(earliest_step_arrival, simple_bounds.earliest_pessimistic_start_times[running_job.job_index]);
			}
			earliest_step_arrival = std::max(earliest_step_arrival, current_time);
			std::cout << "old max load is " << maximum_executed_load << std::endl;
			maximum_executed_load += std::min(problem.num_processors * (next_time - earliest_step_arrival), maximum_load_this_step);
			std::cout << "max load this step is " << maximum_load_this_step << " and max load is " << maximum_executed_load << " and other bound is " << max_load_bound2 << std::endl;
			maximum_executed_load = std::min(maximum_executed_load, max_load_bound2);

			if (minimum_executed_load > maximum_executed_load) certainly_infeasible = true;
			current_time = next_time;
			return true;
		}

		bool is_certainly_infeasible() const {
			return certainly_infeasible;
		}

		Time get_current_time() const {
			return current_time;
		}

		Time get_minimum_executed_load() const {
			return minimum_executed_load;
		}

		Time get_maximum_executed_load() const {
			return maximum_executed_load;
		}
	};
}

#endif
