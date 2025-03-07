#ifndef RECONFIGURATION_OPTIONS_H
#define RECONFIGURATION_OPTIONS_H

namespace NP::Reconfiguration {
	struct SafeSearchOptions {
		int job_skip_chance = 50;
		int history_size = 10;
		double timeout = 0.0;
	};

    struct Options {
		bool enabled = false;
		bool skip_rating_graph = false;
		bool dry_rating_graphs = false;
		double rating_timeout = 0.0;
		int num_threads = 1;
		bool use_z3 = false;
		bool use_cplex = false;
		double feasibility_graph_timeout = 2.0;
		SafeSearchOptions safe_search;
		bool enforce_safe_path = false;
		int max_cuts_per_iteration = 0;
		double enforce_timeout = 0.0;
		bool use_random_analysis = false;
		double minimize_timeout = 0.0;
	};
}

#endif
