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
	public:
		void insert(Finterval<Time> interval) {
			assert(!before && !after);
			middle.push_back(interval);
		}

		void split() {
			assert(!before && !after);
			if (middle.size() < 10) return;

			before = std::make_unique<Interval_tree<Time>>();
			after = std::make_unique<Interval_tree<Time>>();

			std::sort(middle.begin(), middle.end(), [](const Finterval<Time> &a, const Finterval<Time> &b) {
				return a.start + a.end < b.start + b.end;
			});
			const auto &split_interval = middle[middle.size() / 2];
			split_time = (split_interval.start + split_interval.end) / 2;

			std::vector<Finterval<Time>> new_middle;
			for (const auto &interval : middle) {
				if (interval.end <= split_time) before->insert(interval);
				else if (interval.start >= split_time) after->insert(interval);
				else new_middle.push_back(interval);
			}
			middle = new_middle;
			before->split();
			after->split();
		}

		void query(const Finterval<Time> &interval, std::vector<Finterval<Time>> &output) {
			if (before && interval.start < split_time) before->query(interval, output);
			if (after && interval.end > split_time) after->query(interval, output);
			for (const auto &candidate : middle) {
				if (candidate.start < interval.end && candidate.end > interval.start) output.push_back(candidate);
			}
		}
	};
}

#endif
