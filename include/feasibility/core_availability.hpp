#ifndef FEASIBILITY_CORE_AVAILABILITY_H
#define FEASIBILITY_CORE_AVAILABILITY_H

#include <algorithm>
#include <stdexcept>
#include <vector>
#include <cassert>

namespace NP::Feasibility {
	template<class Time> class Core_availability {
		std::vector<Time> finish_times;
		Time last_start_time = 0;

	public:
		explicit Core_availability(int num_cores) {
			if (num_cores < 1) throw std::invalid_argument("num_cores must be positive");
			finish_times.resize(num_cores);
		}

		Time next_start_time() const {
			return std::max(finish_times[0], last_start_time);
		}

		Time second_start_time() const {
			assert(finish_times.size() > 1);
			return std::max(finish_times[1], last_start_time);
		}

		void schedule(Time start, Time duration) {
			if (start < next_start_time()) throw std::invalid_argument("start must be at least next_start_time()");
			finish_times[0] = start + duration;
			std::sort(finish_times.begin(), finish_times.end());
			last_start_time = start;
		}

		void merge(const Core_availability<Time> &source) {
			if (finish_times.size() != source.finish_times.size()) {
				throw std::invalid_argument("source must have the same number of cores");
			}
			for (size_t index = 0; index < finish_times.size(); index++) {
				finish_times[index] = std::max(finish_times[index], source.finish_times[index]);
			}
			last_start_time = std::max(last_start_time, source.last_start_time);
		}

		size_t number_of_processors() const {
			return finish_times.size();
		}
	};
}

#endif
