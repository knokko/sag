#ifndef FEASIBILITY_PACKING_H
#define FEASIBILITY_PACKING_H

#include <vector>

namespace NP::Feasibility {
	template<class Time> static bool is_certainly_unpackable(
		int num_processors, const Time bin_size, std::vector<Time> &jobs
	) {
		assert(num_processors >= 1);
		if (jobs.empty()) return false;

		Time total = 0;
		for (const Time job : jobs) {
			if (job > bin_size) return true;
			total += job;
		}
		if (jobs.size() <= num_processors) return false;
		if (total > num_processors * bin_size) return true;
		if (num_processors == 1) return false;

		std::sort(jobs.begin(), jobs.end());
		if (jobs.size() <= 2) return false;

		if (jobs.size() == 3) {
			assert(num_processors == 2);
			return jobs[0] + jobs[1] > bin_size;
		}
		assert(jobs.size() >= 4);

		const Time smallest2 = std::min(jobs[2], jobs[0] + jobs[1]);
		Time min_wasted_space = 0;
		for (size_t job_index = jobs.size() - 1; job_index > 0; job_index -= 1) {
			const Time duration = jobs[job_index];

			if (duration + jobs[0] > bin_size) {
				min_wasted_space += bin_size - duration;
				continue;
			}

			if (job_index > 1 && duration + jobs[1] > bin_size) {
				assert(duration + jobs[0] <= bin_size);
				min_wasted_space += bin_size - jobs[0] - duration;
				continue;
			}

			if (job_index > 2 && duration + smallest2 > bin_size) {
				assert(duration + jobs[1] <= bin_size);
				min_wasted_space += bin_size - jobs[1] - duration;
			}
		}

		return total + min_wasted_space > num_processors * bin_size;
	}
}

#endif
