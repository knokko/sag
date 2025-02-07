#ifndef RECONFIGURATION_OPTIONS_H
#define RECONFIGURATION_OPTIONS_H

namespace NP::Reconfiguration {
    struct Options {
		bool enabled = false;
		int num_threads = 1;
	};
}

#endif
