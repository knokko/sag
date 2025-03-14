#ifndef TIMEOUT_H
#define TIMEOUT_H

#include <chrono>
#include <iostream>

namespace NP {
	template<class Clock> static bool did_exceed_timeout(double timeout, Clock start_time) {
		const auto current_time = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::ratio<1, 1>> spent_time = current_time - start_time;
		return timeout != 0.0 && spent_time.count() > timeout;
	}

	template<class Clock> static void exit_when_timeout(double timeout, Clock start_time, const char *message) {
		if (did_exceed_timeout(timeout, start_time)) {
			std::cout << message << std::endl;
			exit(0);
		}
	}
}

#endif
