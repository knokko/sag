#ifndef RECONFIGURATION_OPTIONS_H
#define RECONFIGURATION_OPTIONS_H

namespace NP::Reconfiguration {

	#define CUT_ENFORCEMENT_TRADITIONAL 0
	#define CUT_ENFORCEMENT_MODERN_SLOW 1
	#define CUT_ENFORCEMENT_MODERN_FAST 2
	#define CUT_ENFORCEMENT_INSTANT 3

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
		std::string save_job_ordering = "";
		std::string load_job_ordering = "";
		SafeSearchOptions safe_search{};
		int cut_enforcement_strategy = CUT_ENFORCEMENT_MODERN_SLOW;
		double enforce_timeout = 0.0;
		bool use_random_analysis = false;
		bool reverse_tail_analysis = false;
	};
}

#endif
