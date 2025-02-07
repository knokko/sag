#ifndef RECONFIGURATION_CUT_LOOP_H
#define RECONFIGURATION_CUT_LOOP_H

#include "dump.hpp"
#include "problem.hpp"

#include "options.hpp"
#include "rating_graph.hpp"
#include "graph_cutter.hpp"
#include "cut_enforcer.hpp"

#include "feasibility/simple_bounds.hpp"
#include "feasibility/graph.hpp"

using namespace NP;
using namespace NP::Feasibility;

namespace NP::Reconfiguration {

	template<class Time> class Cut_loop {
		const Options &options;
		Scheduling_problem<Time> &problem;
		const size_t num_original_constraints;

		std::vector<Rating_graph_cut> cuts;
		Rating_graph rating_graph;
		bool finished = false;
		bool failed = false;

		void determine_cuts() {
			const auto predecessor_mapping = Feasibility::create_predecessor_mapping(problem);
			Feasibility_graph<Time> feasibility_graph(rating_graph);
			feasibility_graph.explore_forward(problem, compute_simple_bounds(problem), predecessor_mapping);
			feasibility_graph.explore_backward();
			cuts = cut_rating_graph(rating_graph, feasibility_graph);
		}
	public:
		Cut_loop(
			Scheduling_problem<Time> &problem, size_t num_original_constraints, const Options &options
		) : problem(problem), num_original_constraints(num_original_constraints), options(options) {
			Agent_rating_graph<Time>::generate(problem, rating_graph);
			double root_rating = rating_graph.nodes[0].get_rating();
			if (root_rating == 0.0 || root_rating == 1.0) finished = true;
			if (root_rating == 0.0 || compute_simple_bounds(problem).definitely_infeasible) failed = true;
			if (!failed) determine_cuts();
		}

		void cut_until_finished(bool print_progress, int max_num_cuts) {
			assert(max_num_cuts >= 1);
			int next_num_cuts = 1;

			while (!finished) {
				assert(next_num_cuts <= max_num_cuts);
				if (attempt_next(next_num_cuts)) {
					if (print_progress) std::cout << "Successfully performed up to " << next_num_cuts << " cuts; remaining #cuts is " << cuts.size() << std::endl;
					next_num_cuts = std::min(2 * next_num_cuts, max_num_cuts);
				} else {
					if (next_num_cuts == 1) {
						dump_problem("failed-cut-loop.csv", "failed-cut-loop.prec.csv", problem);
						throw std::runtime_error("Failed to perform 1 cut, which should be impossible. Dumped problem at failed-cut-loop(.prec).csv");
					} else {
						if (print_progress) std::cout << "Failed to perform " << next_num_cuts << " cuts at the same time; slowing down" << std::endl;
						next_num_cuts /= 2;
					}
				}
			}
			if (print_progress) std::cout << "Finished performing cuts by using " << (problem.prec.size() - num_original_constraints) << " constraints" << std::endl;
		}

		bool attempt_next(int max_num_cuts) {
			assert(!finished);
			const size_t previous_num_constraints = problem.prec.size();

			auto next_cuts = cuts;
			if (next_cuts.size() > max_num_cuts) next_cuts.resize(max_num_cuts);
			enforce_cuts(problem, rating_graph, next_cuts, compute_simple_bounds(problem));

			bool failed_attempt = false;
			if (compute_simple_bounds(problem).definitely_infeasible) failed_attempt = true;
			else {
				Rating_graph new_rating_graph;
				Agent_rating_graph<Time>::generate(problem, new_rating_graph);
				if (new_rating_graph.nodes[0].get_rating() == 0.0) failed_attempt = true;
				else if (new_rating_graph.nodes[0].get_rating() == 1.0) finished = true;
				else {
					rating_graph = new_rating_graph;
					auto backup_cuts = cuts;
					determine_cuts();
					for (const auto &cut : cuts) {
						if (cut.safe_jobs.empty()) failed_attempt = true;
					}
					if (failed_attempt) cuts = backup_cuts;
				}
			}

			if (failed_attempt) problem.prec.resize(previous_num_constraints, Precedence_constraint<Time>(problem.jobs[0].get_id(), problem.jobs[0].get_id(), Interval<Time>()));
			return !failed_attempt;
		}

		bool has_finished() const {
			return finished;
		}

		bool has_failed() const {
			return failed;
		}
	};
}

#endif
