#ifndef FEASIBILITY_FROM_SCRATCH_HPP
#define FEASIBILITY_FROM_SCRATCH_HPP

#include <atomic>
#include <cstdlib>
#include <future>
#include <iostream>
#include <mutex>

#include "problem.hpp"
#include "simple_bounds.hpp"
#include "node.hpp"

namespace NP::Feasibility {
	
	template<class Time> class Ordering_generator {
		const Scheduling_problem<Time> &problem;
		const Simple_bounds<Time> &bounds;
		const std::vector<std::vector<Precedence_constraint<Time>>> &predecessor_mapping;
		const int skip_chance;

		Active_node<Time> node;
		Index_set dispatched_jobs;
		size_t slack_job_index = 0;
		std::vector<Job_index> jobs_by_slack;
		size_t finish_job_index = 0;
		std::vector<Job_index> jobs_by_finish_time;
		std::vector<int> remaining_predecessors;
		std::vector<std::vector<Precedence_constraint<Time>>> successor_mapping;
		bool failed = false;

		void update_slack_job_index() {
			while (slack_job_index < jobs_by_slack.size() && dispatched_jobs.contains(jobs_by_slack[slack_job_index])) slack_job_index += 1;
			if (slack_job_index >= jobs_by_slack.size()) {
				for (int remaining : remaining_predecessors) assert(remaining == 0);
			}
		}

		bool can_dispatch(Job_index job_index) const {
			if (remaining_predecessors[job_index] > 0) return false;
			if (dispatched_jobs.contains(job_index)) return false;

			if (job_index == jobs_by_slack[slack_job_index]) {
				size_t next_slack_index = slack_job_index + 1;
				while (next_slack_index < problem.jobs.size() && dispatched_jobs.contains(jobs_by_slack[next_slack_index])) {
					next_slack_index += 1;
				}
				if (next_slack_index < problem.jobs.size() &&
						node.predict_next_start_time(problem.jobs[job_index], predecessor_mapping) > bounds.latest_safe_start_times[jobs_by_slack[next_slack_index]]) return false;
			} else {
				if (node.predict_start_time(problem.jobs[job_index], predecessor_mapping) > bounds.latest_safe_start_times[jobs_by_slack[slack_job_index]]) return false;
			}

			return true;
		}
	public:
		Ordering_generator(
			const Scheduling_problem<Time> &problem,
			const Simple_bounds<Time> &bounds,
			const std::vector<std::vector<Precedence_constraint<Time>>> &predecessor_mapping,
			int skip_chance
		) : problem(problem), bounds(bounds), predecessor_mapping(predecessor_mapping), node(problem.num_processors), skip_chance(skip_chance) {
			assert(skip_chance >= 0);
			assert(skip_chance < 100);
			jobs_by_slack.reserve(problem.jobs.size());
			jobs_by_finish_time.reserve(problem.jobs.size());
			remaining_predecessors.reserve(problem.jobs.size());
			successor_mapping.reserve(problem.jobs.size());

			for (Job_index job_index = 0; job_index < problem.jobs.size(); job_index++) {
				jobs_by_slack.push_back(job_index);
				jobs_by_finish_time.push_back(job_index);
				remaining_predecessors.emplace_back();
				successor_mapping.emplace_back();
			}
			std::sort(jobs_by_slack.begin(), jobs_by_slack.end(), [&bounds](const Job_index &a, const Job_index &b) {
				return bounds.latest_safe_start_times[a] < bounds.latest_safe_start_times[b];
			});
			std::sort(jobs_by_finish_time.begin(), jobs_by_finish_time.end(), [&bounds, &problem](const Job_index &a, const Job_index &b) {
				return bounds.earliest_pessimistic_start_times[a] + problem.jobs[a].maximal_exec_time() <
						bounds.earliest_pessimistic_start_times[b] + problem.jobs[b].maximal_exec_time();
			});

			for (const auto &constraint : problem.prec) {
				remaining_predecessors[constraint.get_toIndex()] += 1;
				successor_mapping[constraint.get_fromIndex()].push_back(constraint);
			}
		}

		bool has_failed() const {
			return failed;
		}

		bool has_finished() const {
			return failed || slack_job_index >= jobs_by_slack.size();
		}

		Job_index choose_next_job() {
			update_slack_job_index();
			assert(slack_job_index < jobs_by_slack.size());

			// The next job must be dispatched before or at start_deadline
			if (node.next_core_available() > bounds.latest_safe_start_times[jobs_by_slack[slack_job_index]]) {
				failed = true;
				return jobs_by_slack[slack_job_index];
			}

			size_t valid_slack_index = slack_job_index;
			while (!can_dispatch(jobs_by_slack[valid_slack_index])) {
				valid_slack_index += 1;
				if (valid_slack_index == problem.jobs.size()) {
					failed = true;
					return jobs_by_slack[slack_job_index];
				}
			}

			size_t candidate_slack_index = valid_slack_index;
			while (candidate_slack_index < problem.jobs.size()) {
				if (can_dispatch(jobs_by_slack[candidate_slack_index]) && rand() % 100 >= skip_chance) break;
				candidate_slack_index += 1;
			}
			if (candidate_slack_index == problem.jobs.size()) candidate_slack_index = valid_slack_index;

			Job_index next_job = jobs_by_slack[candidate_slack_index];
			Time next_start_time = node.predict_start_time(problem.jobs[next_job], predecessor_mapping);

			while (finish_job_index < problem.jobs.size() && dispatched_jobs.contains(jobs_by_finish_time[finish_job_index])) finish_job_index += 1;
			size_t candidate_finish_index = finish_job_index;
			while (candidate_finish_index < problem.jobs.size() && !can_dispatch(jobs_by_finish_time[candidate_finish_index])) candidate_finish_index += 1;

			if (candidate_finish_index < problem.jobs.size()) {
				Job_index quick_job = jobs_by_finish_time[candidate_finish_index];
				if (node.predict_start_time(problem.jobs[quick_job], predecessor_mapping) + problem.jobs[quick_job].maximal_exec_time() <= next_start_time) {
					next_job = quick_job;
				}
			}

			dispatched_jobs.add(next_job);
			node.schedule(problem.jobs[next_job], bounds, predecessor_mapping);

			for (const auto &successor : successor_mapping[next_job]) {
				assert(remaining_predecessors[successor.get_toIndex()] > 0);
				remaining_predecessors[successor.get_toIndex()] -= 1;
			}
			successor_mapping[next_job].clear();
			update_slack_job_index();
			return next_job;
		}
	};

	template<class Time> static std::vector<Job_index> launch_job_ordering_search_thread(
		const Scheduling_problem<Time> &problem,
		const Simple_bounds<Time> &bounds,
		const std::vector<std::vector<Precedence_constraint<Time>>> &predecessor_mapping,
		int skip_chance, bool print_progress,
		std::shared_ptr<std::atomic<size_t>> high_score,
		std::shared_ptr<std::atomic<bool>> is_finished,
		std::shared_ptr<std::mutex> lock
	) {
		std::vector<Job_index> result;
		result.reserve(problem.jobs.size());
		while (!is_finished->load()) {
			result.clear();

			Ordering_generator<Time> random_generator(problem, bounds, predecessor_mapping, skip_chance);
			while (!random_generator.has_finished()) result.push_back(random_generator.choose_next_job());
			if (!random_generator.has_failed()) {
				bool is_first = false;
				lock->lock();
				if (!is_finished->load()) {
					is_finished->store(true);
					is_first = true;
				}
				lock->unlock();
				if (is_first) return result;
			}

			if (result.size() > high_score->load() && !is_finished->load()) {
				lock->lock();
				if (result.size() > high_score->load()) {
					high_score->store(result.size());
					if (print_progress) std::cout << "Failed after " << result.size() << " / " << problem.jobs.size() << " jobs" << std::endl;
				}
				lock->unlock();
			}
		}
		return {};
	}

	template<class Time> static std::vector<Job_index> search_for_safe_job_ordering(
		const Scheduling_problem<Time> &problem,
		const Simple_bounds<Time> &bounds,
		const std::vector<std::vector<Precedence_constraint<Time>>> &predecessor_mapping,
		int skip_chance, size_t num_threads, bool print_progress
	) {
		{
			// First try simple least-slack-first scheduling
			std::vector<Job_index> result;
			result.reserve(problem.jobs.size());

			Ordering_generator<Time> simple_generator(problem, bounds, predecessor_mapping, 0);
			while (!simple_generator.has_finished()) result.push_back(simple_generator.choose_next_job());
			if (!simple_generator.has_failed()) return result;
		}

		assert(skip_chance > 0 && num_threads > 0);
		auto high_score = std::make_shared<std::atomic<size_t>>(0);
		auto is_finished = std::make_shared<std::atomic<bool>>(false);
		auto lock = std::make_shared<std::mutex>();
		std::vector<std::future<std::vector<Job_index>>> trial_threads;
		trial_threads.reserve(num_threads);
		for (int counter = 0; counter < num_threads; counter++) {
			trial_threads.push_back(std::async(
				std::launch::async, &launch_job_ordering_search_thread<Time>, problem, bounds, predecessor_mapping,
				skip_chance, print_progress, high_score, is_finished, lock
			));
		}

		std::vector<Job_index> result;
		result.reserve(problem.jobs.size());
		for (auto &thread : trial_threads) {
			const auto thread_result = thread.get();
			for (const Job_index job : thread_result) result.push_back(job);
		}
		assert(result.size() == problem.jobs.size());
		return result;
	}

	template<class Time> static void enforce_safe_job_ordering(
		Scheduling_problem<Time> &problem, std::vector<Job_index> safe_ordering
	) {
		for (size_t index = 1; index < safe_ordering.size(); index++) {
			problem.prec.emplace_back(problem.jobs[safe_ordering[index - 1]].get_id(), problem.jobs[safe_ordering[index]].get_id(), Interval<Time>(), false);
		}
		validate_prec_cstrnts<Time>(problem.prec, problem.jobs);
	}
}

#endif
