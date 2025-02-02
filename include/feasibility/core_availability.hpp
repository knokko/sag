#ifndef FEASIBILITY_CORE_AVAILABILITY_H
#define FEASIBILITY_CORE_AVAILABILITY_H

#include <algorithm>
#include <stdexcept>
#include <vector>
#include <cassert>

namespace NP::Feasibility {
	template<class Time> class Core_availability {
		std::vector<Time> finish_times;

	public:
		explicit Core_availability(int num_cores) {
			if (num_cores < 1) throw std::invalid_argument("num_cores must be positive");
			finish_times.resize(num_cores);
		}

		Time next_start_time() const {
			return finish_times[0];
		}

		Time second_start_time() const {
			assert(finish_times.size() > 1);
			return finish_times[1];
		}

		void schedule(Time start, Time duration) {
			if (start < next_start_time()) throw std::invalid_argument("start must be at least next_start_time()");
			finish_times[0] = start + duration;
			std::sort(finish_times.begin(), finish_times.end());
		}

		void merge(const Core_availability<Time> &source) {
			if (finish_times.size() != source.finish_times.size()) {
				throw std::invalid_argument("source must have the same number of cores");
			}
			for (size_t index = 0; index < finish_times.size(); index++) {
				finish_times[index] = std::max(finish_times[index], source.finish_times[index]);
			}
		}

		size_t number_of_processors() const {
			return finish_times.size();
		}
	};
}

#endif
