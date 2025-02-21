#ifndef RECONFIGURATION_CUT_LOOP_H
#define RECONFIGURATION_CUT_LOOP_H

#include "problem.hpp"

#include "rating_graph.hpp"
#include "graph_cutter.hpp"
#include "cut_enforcer.hpp"

using namespace NP;
using namespace NP::Feasibility;

namespace NP::Reconfiguration {

	template<class Time> class Cut_loop {
		Scheduling_problem<Time> &problem;
		std::vector<Job_index> safe_path;
	public:
		Cut_loop(Scheduling_problem<Time> &problem, std::vector<Job_index> safe_path) : problem(problem), safe_path(safe_path) {
			assert(!problem.jobs.empty());
		}

		void cut_until_finished(bool print_progress, int max_num_cuts, bool dry_rating_runs) {
			const size_t num_original_constraints = problem.prec.size();
			while (true) {
				Rating_graph rating_graph;
				Agent_rating_graph<Time>::generate(problem, rating_graph, dry_rating_runs);
				if (rating_graph.nodes[0].get_rating() == 1.0) break;
				const auto cuts = cut_rating_graph(rating_graph, safe_path);
				assert(!cuts.empty());
				enforce_cuts_with_path(problem, cuts, safe_path, max_num_cuts);
				if (print_progress) std::cout << " increased #extra constraints to " << (problem.prec.size() - num_original_constraints) << std::endl;
			}
		}
	};
}

#endif
