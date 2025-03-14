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

	template<class Clock> static void exit_when_timeout(
		double timeout, Clock start_time, const char *message, FILE *file_to_remove = nullptr, const char *path_to_remove = nullptr
	) {
		if (did_exceed_timeout(timeout, start_time)) {
			std::cout << message << std::endl;
			if (file_to_remove) fclose(file_to_remove);
			if (path_to_remove) std::remove(path_to_remove);
			exit(0);
		}
	}
}

#endif
