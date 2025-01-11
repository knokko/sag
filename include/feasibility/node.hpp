#ifndef FEASIBILITY_NODE_H
#define FEASIBILITY_NODE_H

#include "index_set.hpp"
#include "core_availability.hpp"
#include "simple_bounds.hpp"

namespace NP::Feasibility {

	template<class Time> std::vector<std::vector<Precedence_constraint<Time>>> create_predecessor_mapping(
			const Scheduling_problem<Time> &problem
	) {
		std::vector<std::vector<Precedence_constraint<Time>>> mapping(problem.jobs.size());
		for (const auto &precedence_constraint : problem.prec) {
			mapping[precedence_constraint.get_toIndex()].push_back(precedence_constraint);
		}
		return mapping;
	}

	template<class Time> struct Running_job {
		Job_index job_index;
		Time started_at;
		Time finishes_at;
	};

	template<class Time> class Active_node {
		Index_set finished_jobs;
		std::vector<Running_job<Time>> running_jobs;
		Core_availability<Time> core_availability;

		bool missed_deadline = false;
	public:
		explicit Active_node(int num_cores) : core_availability(Core_availability<Time>(num_cores)) {}

		void schedule(
				const Job<Time> &job,
				const Simple_bounds<Time> &bounds,
				const std::vector<std::vector<Precedence_constraint<Time>>> &predecessor_mapping
		) {
			Time ready_time = job.latest_arrival();
			for (const auto &precedent_constraint: predecessor_mapping[job.get_job_index()]) {
				if (finished_jobs.contains(precedent_constraint.get_fromIndex())) continue;

				bool found_it = false;
				for (const auto &running_job : running_jobs) {
					if (running_job.job_index == precedent_constraint.get_fromIndex()) {
						Time ready_bound = precedent_constraint.get_maxsus();
						if (precedent_constraint.should_signal_at_completion()) ready_bound += running_job.finishes_at;
						else ready_bound += running_job.started_at;
						ready_time = std::max(ready_time, ready_bound);
						found_it = true;
						break;
					}
				}

				if (!found_it) throw std::invalid_argument("Not all predecessors have been scheduled yet");
			}

			const Time start_time = std::max(ready_time, core_availability.next_start_time());
			if (start_time > bounds.latest_safe_start_times[job.get_job_index()]) missed_deadline = true;
			assert(start_time >= bounds.earliest_pessimistic_start_times[job.get_job_index()]);

			core_availability.schedule(start_time, job.maximal_exec_time());

			for (size_t running_index = 0; running_index < running_jobs.size(); running_index++) {
				const auto &running_job = running_jobs[running_index];
				if (start_time >= running_job.finishes_at + bounds.maximum_suspensions[running_job.job_index]) {
					assert(!finished_jobs.contains(running_job.job_index));
					finished_jobs.add(running_job.job_index);

					if (running_index != running_jobs.size() - 1) {
						std::swap(running_jobs[running_index], running_jobs[running_jobs.size() - 1]);
						running_index -= 1;
					}
					running_jobs.resize(running_jobs.size() - 1);
				}
			}
			running_jobs.push_back(Running_job<Time> {
				.job_index=job.get_job_index(),
				.started_at=start_time,
				.finishes_at=start_time + job.maximal_exec_time()
			});
		}

		void merge(const Active_node<Time> &other) {
			core_availability.merge(other.core_availability);

			for (const auto &other_running_job : other.running_jobs) {
				bool found_my_running_job = false;
				for (auto &my_running_job : running_jobs) {
					if (my_running_job.job_index == other_running_job.job_index) {
						my_running_job.started_at = std::max(my_running_job.started_at, other_running_job.started_at);
						my_running_job.finishes_at = std::max(my_running_job.finishes_at, other_running_job.finishes_at);
						found_my_running_job = true;
						break;
					}
				}

				if (!found_my_running_job) {
					if (!finished_jobs.contains(other_running_job.job_index)) {
						throw std::invalid_argument("Incompatible scheduled job history: the set of scheduled jobs must be equal");
					}
					finished_jobs.remove(other_running_job.job_index);
					running_jobs.push_back(other_running_job);
				}
			}
		}

		std::unique_ptr<Active_node<Time>> copy() const {
			auto copied = std::make_unique<Active_node<Time>>(core_availability.number_of_processors());
			copied->running_jobs = running_jobs;
			copied->finished_jobs.copy_from(finished_jobs);
			copied->core_availability = core_availability;
			copied->missed_deadline = missed_deadline;
			return copied;
		}

		bool has_missed_deadline() const {
			return missed_deadline;
		}

		size_t get_num_dispatched_jobs() const {
			return finished_jobs.size() + running_jobs.size();
		}
	};
}

#endif
