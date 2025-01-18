#ifndef FEASIBILITY_INTERVAL_TREE_H
#define FEASIBILITY_INTERVAL_TREE_H

#include "problem.hpp"

#include "simple_bounds.hpp"

#include <iostream>

namespace NP::Feasibility {
	template<class Time> struct Finterval {
		Job_index job_index;
		Time start;
		Time end;
	};

	template<class Time> class Interval_tree {
		Time split_time;
		std::vector<Finterval<Time>> middle;

		std::unique_ptr<Interval_tree<Time>> before;
		std::unique_ptr<Interval_tree<Time>> after;

		std::vector<std::unique_ptr<Interval_tree<Time>>*> stack;
	public:
		void insert(Finterval<Time> interval) {
			assert(!before && !after);
			middle.push_back(interval);
		}

		void split() {
			assert(!before && !after);
			if (middle.size() < 50) return;

			before = std::make_unique<Interval_tree<Time>>();
			after = std::make_unique<Interval_tree<Time>>();

			std::sort(middle.begin(), middle.end(), [](const Finterval<Time> &a, const Finterval<Time> &b) {
				return a.start + a.end < b.start + b.end;
			});
			const auto &split_interval = middle[middle.size() / 2];
			split_time = (split_interval.start + split_interval.end) / 2;

			{
				std::vector<Finterval<Time>> new_middle;
				for (const auto &interval : middle) {
					if (interval.end <= split_time) before->insert(interval);
					else if (interval.start >= split_time) after->insert(interval);
					else new_middle.push_back(interval);
				}
				middle = new_middle;
				middle.shrink_to_fit();
			}

			before->split();
			after->split();
		}

		void query(const Finterval<Time> &interval, std::vector<Finterval<Time>> &output) {
			assert(stack.empty());
			if (before && interval.start < split_time) stack.push_back(&before);
			if (after && interval.end > split_time) stack.push_back(&after);
			for (const auto &candidate : middle) {
				if (candidate.start < interval.end && candidate.end > interval.start) output.push_back(candidate);
			}

			while (!stack.empty()) {
				const std::unique_ptr<Interval_tree<Time>> *raw_current_node = stack[stack.size() - 1];
				const std::unique_ptr<Interval_tree<Time>> &current_node = *raw_current_node;
				stack.pop_back();

				if (current_node->before && interval.start < current_node->split_time) stack.push_back(&current_node->before);
				if (current_node->after && interval.end > current_node->split_time) stack.push_back(&current_node->after);
				for (const auto &candidate : current_node->middle) {
					if (candidate.start < interval.end && candidate.end > interval.start) output.push_back(candidate);
				}
			}
			stack.clear();
		}
	};
}

#endif
