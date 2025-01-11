#ifndef FEASIBILITY_SIMPLE_BOUNDS_H
#define FEASIBILITY_SIMPLE_BOUNDS_H

#include "problem.hpp"

namespace NP::Feasibility {

	template<class Time> static std::vector<Job_index> find_cycle(const Scheduling_problem<Time> &problem) {
		std::vector<std::vector<Job_index>> adjacency_list(problem.jobs.size(), std::vector<Job_index>());

		for (size_t constraint_index = 0; constraint_index < problem.prec.size(); constraint_index++) {
			const auto &precedence_constraint = problem.prec[constraint_index];
			adjacency_list[precedence_constraint.get_fromIndex()].push_back(precedence_constraint.get_toIndex());
		}

		struct StackEntry {
			Job_index job_index;
			size_t successor_index;
		};

		std::vector<bool> has_seen_before(problem.jobs.size(), false);
		for (Job_index starting_job = 0; starting_job < problem.jobs.size(); starting_job++) {
			if (has_seen_before[starting_job]) continue;

			std::vector<bool> is_part_of_recursion_stack(problem.jobs.size(), false);
			std::vector<StackEntry> recursion_stack{StackEntry { .job_index=starting_job, .successor_index=0 }};

			while (!recursion_stack.empty()) {
				auto &current_entry = recursion_stack[recursion_stack.size() - 1];
				has_seen_before[current_entry.job_index] = true;

				if (is_part_of_recursion_stack[current_entry.job_index] && current_entry.successor_index == 0) {
					std::vector<Job_index> problematic_chain;
					bool has_started_cycle = false;
					for (const auto &entry : recursion_stack) {
						if (!has_started_cycle && entry.job_index == current_entry.job_index) has_started_cycle = true;
						if (has_started_cycle) problematic_chain.push_back(entry.job_index);
					}
					return problematic_chain;
				}

				is_part_of_recursion_stack[current_entry.job_index] = true;

				const auto &successors = adjacency_list[current_entry.job_index];
				if (current_entry.successor_index < successors.size()) {
					Job_index successor_job = successors[current_entry.successor_index];
					current_entry.successor_index += 1;
					recursion_stack.push_back(StackEntry { .job_index=successor_job, .successor_index=0 });
				} else {
					is_part_of_recursion_stack[current_entry.job_index] = false;
					recursion_stack.pop_back();
				}
			}
		}

		return {};
	}

	template<class Time> static std::vector<Job_index> find_critical_path(
			const Scheduling_problem<Time> &problem, Job_index problematic_job_index,
			const std::vector<Time> &earliest_pessimistic_start_times
	) {
		std::vector<std::vector<Precedence_constraint<Time>>> back_edges(
				problem.jobs.size(), std::vector<Precedence_constraint<Time>>()
		);
		for (const auto &precedence_constraint : problem.prec) {
			back_edges[precedence_constraint.get_toIndex()].push_back(precedence_constraint);
		}

		std::vector<Job_index> critical_path{problematic_job_index};
		Job_index current_job_index = problematic_job_index;
		while (true) {
			Time earliest_start = earliest_pessimistic_start_times[current_job_index];
			if (earliest_start == problem.jobs[current_job_index].latest_arrival()) break;

			for (const auto &precedence_constraint : back_edges[current_job_index]) {
				Job_index other_index = precedence_constraint.get_fromIndex();
				Time constraint_bound = earliest_pessimistic_start_times[other_index] + precedence_constraint.get_maxsus();
				if (precedence_constraint.should_signal_at_completion()) constraint_bound += problem.jobs[other_index].maximal_exec_time();
				if (constraint_bound == earliest_start) {
					critical_path.push_back(other_index);
					current_job_index = other_index;
					break;
				}
			}
		}
		std::reverse(critical_path.begin(), critical_path.end());
		return critical_path;
	}

	template<class Time> struct Simple_bounds {
		bool has_precedence_cycle{};
		bool definitely_infeasible{};
		std::vector<Time> earliest_pessimistic_start_times;
		std::vector<Time> latest_safe_start_times;
		std::vector<Time> maximum_suspensions;
		std::vector<Job_index> problematic_chain;
	};

	template<class Time> static std::vector<Time> compute_latest_safe_start_times(
			const Scheduling_problem<Time> &problem
	) {
		struct JobArrivalBuilder {
			Job_index job;
			Time latest_safe_start_time;
			int remaining_successors;
			std::vector<Precedence_constraint<Time>> predecessors;
			int count_predecessors;
		};

		std::vector<JobArrivalBuilder> building_job_arrivals;
		building_job_arrivals.reserve(problem.jobs.size());
		for (const auto &job : problem.jobs) {
			building_job_arrivals.push_back(JobArrivalBuilder {
					.job=job.get_job_index(),
					.latest_safe_start_time=job.get_deadline() - job.maximal_exec_time(),
					.remaining_successors=0,
					.predecessors=std::vector<Precedence_constraint<Time>>()
			});
		}

		for (const auto &constraint : problem.prec) {
			building_job_arrivals[constraint.get_fromIndex()].remaining_successors += 1;
			building_job_arrivals[constraint.get_toIndex()].count_predecessors += 1;
		}

		for (auto &arrival : building_job_arrivals) {
			arrival.predecessors.reserve(arrival.count_predecessors);
		}

		for (const auto &constraint : problem.prec) {
			building_job_arrivals[constraint.get_toIndex()].predecessors.push_back(constraint);
		}

		Job_index completed_job_count = 0;
		std::vector<Job_index> next_jobs;
		for (const auto &arrival : building_job_arrivals) {
			if (arrival.remaining_successors == 0) next_jobs.push_back(arrival.job);
		}

		while (!next_jobs.empty()) {
			auto &next_job = building_job_arrivals[next_jobs[next_jobs.size() - 1]];
			next_jobs.pop_back();
			for (const auto &constraint : next_job.predecessors) {
				auto &predecessor = building_job_arrivals[constraint.get_fromIndex()];
				predecessor.remaining_successors -= 1;
				assert(predecessor.remaining_successors >= 0);
				Time time_gap = constraint.get_maxsus();
				if (constraint.should_signal_at_completion()) time_gap += problem.jobs[predecessor.job].maximal_exec_time();
				if (time_gap > next_job.latest_safe_start_time) {
					predecessor.latest_safe_start_time = 0;
				} else {
					predecessor.latest_safe_start_time = std::min(
							predecessor.latest_safe_start_time, next_job.latest_safe_start_time - time_gap
					);
				}
				if (predecessor.remaining_successors == 0) next_jobs.push_back(predecessor.job);
			}
			completed_job_count += 1;
		}

		assert(completed_job_count == problem.jobs.size());

		std::vector<Time> latest_safe_start_times;
		latest_safe_start_times.reserve(problem.jobs.size());
		for (const auto &builder : building_job_arrivals) {
			latest_safe_start_times.push_back(builder.latest_safe_start_time);
		}

		return latest_safe_start_times;
	}

	template<class Time> static Simple_bounds<Time> compute_simple_bounds(const Scheduling_problem<Time> &problem) {
		struct JobArrivalBuilder {
			Job_index job;
			Time earliest_pessimistic_start;
			int remaining_predecessors;
			std::vector<Precedence_constraint<Time>> successors;
			int count_successors;
		};

		std::vector<JobArrivalBuilder> building_job_arrivals;
		building_job_arrivals.reserve(problem.jobs.size());
		for (const auto &job : problem.jobs) {
			building_job_arrivals.push_back(JobArrivalBuilder {
					.job=job.get_job_index(),
					.earliest_pessimistic_start=job.latest_arrival(),
					.remaining_predecessors=0,
					.successors=std::vector<Precedence_constraint<Time>>()
			});
		}

		for (const auto &constraint : problem.prec) {
			building_job_arrivals[constraint.get_fromIndex()].count_successors += 1;
			building_job_arrivals[constraint.get_toIndex()].remaining_predecessors += 1;
		}

		for (auto &arrival : building_job_arrivals) {
			arrival.successors.reserve(arrival.count_successors);
		}

		for (const auto &constraint : problem.prec) {
			building_job_arrivals[constraint.get_fromIndex()].successors.push_back(constraint);
		}

		Job_index completed_job_count = 0;
		std::vector<Job_index> next_jobs;
		for (const auto &arrival : building_job_arrivals) {
			if (arrival.remaining_predecessors == 0) next_jobs.push_back(arrival.job);
		}

		while (!next_jobs.empty()) {
			auto &next_job = building_job_arrivals[next_jobs[next_jobs.size() - 1]];
			next_jobs.pop_back();
			const auto earliest_pessimistic_completion = next_job.earliest_pessimistic_start + problem.jobs[next_job.job].maximal_exec_time();
			for (const auto &constraint : next_job.successors) {
				auto &successor = building_job_arrivals[constraint.get_toIndex()];
				successor.remaining_predecessors -= 1;
				assert(successor.remaining_predecessors >= 0);

				Time other_bound = constraint.get_maxsus();
				if (constraint.should_signal_at_completion()) other_bound += earliest_pessimistic_completion;
				else other_bound += next_job.earliest_pessimistic_start;
				successor.earliest_pessimistic_start = std::max(successor.earliest_pessimistic_start, other_bound);
				if (successor.remaining_predecessors == 0) next_jobs.push_back(successor.job);
			}
			completed_job_count += 1;
		}

		if (completed_job_count != problem.jobs.size()) {
			assert(completed_job_count < problem.jobs.size());
			return Simple_bounds<Time> { .has_precedence_cycle=true, .definitely_infeasible=true, .problematic_chain=find_cycle(problem) };
		}

		std::vector<Time> earliest_pessimistic_start_times;
		earliest_pessimistic_start_times.reserve(problem.jobs.size());
		for (const auto &builder : building_job_arrivals) {
			earliest_pessimistic_start_times.push_back(builder.earliest_pessimistic_start);
		}

		std::vector<Job_index> problematic_chain;
		for (Job_index job_index = 0; job_index < problem.jobs.size(); job_index++) {
			const auto &job = problem.jobs[job_index];
			if (earliest_pessimistic_start_times[job_index] + job.maximal_exec_time() > job.get_deadline()) {
				problematic_chain = find_critical_path(problem, job_index, earliest_pessimistic_start_times);
				break;
			}
		}

		std::vector<Time> maximum_suspensions(problem.jobs.size(), 0);
		for (const auto &precedence_constraint : problem.prec) {
			Job_index job_index = precedence_constraint.get_fromIndex();
			maximum_suspensions[job_index] = std::max(precedence_constraint.get_maxsus(), maximum_suspensions[job_index]);
		}

		return Simple_bounds<Time> {
			.has_precedence_cycle=false,
			.definitely_infeasible=!problematic_chain.empty(),
			.earliest_pessimistic_start_times=earliest_pessimistic_start_times,
			.latest_safe_start_times=compute_latest_safe_start_times(problem),
			.maximum_suspensions=maximum_suspensions,
			.problematic_chain=problematic_chain
		};
	}
}

#endif
