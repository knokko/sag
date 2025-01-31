#ifndef RECONFIGURATION_CUT_ENFORCER_H
#define RECONFIGURATION_CUT_ENFORCER_H

#include "rating_graph.hpp"
#include "job_orderings.hpp"
#include "problem.hpp"
#include "feasibility/simple_bounds.hpp"

namespace NP::Reconfiguration {
	template<class Time> static void enforce_cuts(
		NP::Scheduling_problem<Time> &problem, Rating_graph &rating_graph,
		std::vector<Rating_graph_cut> &cuts, const Feasibility::Simple_bounds<Time> &feasibility_bounds
	) {
		const size_t num_original_constraints = problem.prec.size();
		for (auto &cut : cuts) {
			if (cut.safe_jobs.empty()) return;
		}

		const auto cut_orderings = determine_orderings_for_cuts(problem.jobs.size(), rating_graph, cuts);

		for (size_t cut_index = 0; cut_index < 1; cut_index++) {
			auto &cut = cuts[cut_index];
			const auto &job_orderings = cut_orderings[cut_index];
			bool is_cut_unreachable = false;
			for (size_t existing_index = num_original_constraints; existing_index < problem.prec.size(); existing_index++) {
				const auto &constraint = problem.prec[existing_index];
				if (job_orderings.get_earliest_occurrence_index(constraint.get_fromIndex()) >= job_orderings.get_latest_occurrence_index(constraint.get_toIndex())) {
					is_cut_unreachable = true;
					break;
				}
			}
			if (is_cut_unreachable) continue;


			// TODO Be smarter than just sorting
			std::sort(cut.safe_jobs.begin(), cut.safe_jobs.end(), [&feasibility_bounds](const Job_index &a, const Job_index &b) {
				return feasibility_bounds.earliest_pessimistic_start_times[a] < feasibility_bounds.earliest_pessimistic_start_times[b];
			});

			const Job_index safe_job_index = cut.safe_jobs[0];
			for (const Job_index forbidden_job_index : cut.forbidden_jobs) {
				bool is_already_enforced = false;
				// TODO Make this much more powerful
				for (size_t existing_index = num_original_constraints; existing_index < problem.prec.size(); existing_index++) {
					const auto &constraint = problem.prec[existing_index];
					if (constraint.get_fromIndex() == safe_job_index && constraint.get_toIndex() == forbidden_job_index) {
						is_already_enforced = true;
						break;
					}
				}

				if (!is_already_enforced) {
					Precedence_constraint<Time> new_constraint(
						problem.jobs[safe_job_index].get_id(), problem.jobs[forbidden_job_index].get_id(), Interval<Time>(), false
					);
					new_constraint.set_fromIndex(safe_job_index);
					new_constraint.set_toIndex(forbidden_job_index);
					problem.prec.push_back(new_constraint);
				}
			}
		}
		validate_prec_cstrnts<Time>(problem.prec, problem.jobs);
	}
}

#endif
