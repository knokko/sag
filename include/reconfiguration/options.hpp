#ifndef RECONFIGURATION_OPTIONS_H
#define RECONFIGURATION_OPTIONS_H

namespace NP::Reconfiguration {
    struct Options {
		bool enabled = false;
		int num_threads = 1;
		int max_cuts_per_iteration = 0;
	};
}

#endif
