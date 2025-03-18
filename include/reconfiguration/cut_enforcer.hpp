#ifndef RECONFIGURATION_CUT_ENFORCER_H
#define RECONFIGURATION_CUT_ENFORCER_H

#include "options.hpp"
#include "rating_graph.hpp"
#include "job_orderings.hpp"
#include "problem.hpp"
#include "dump.hpp"
#include "feasibility/simple_bounds.hpp"

namespace NP::Reconfiguration {
	template<class Time> static void enforce_cuts(
		NP::Scheduling_problem<Time> &problem,
		const std::vector<Rating_graph_cut> &cuts,
		const std::vector<Job_index> &safe_path,
		int strategy
	) {
		assert(!cuts.empty());
		assert(safe_path.size() == problem.jobs.size());

		if (strategy == CUT_ENFORCEMENT_MODERN_SLOW || strategy == CUT_ENFORCEMENT_MODERN_FAST) {
			std::vector<size_t> path_indices(safe_path.size(), -1);
			for (size_t index = 0; index < safe_path.size(); index++) {
				path_indices[safe_path[index]] = index;
			}
			for (const size_t index : path_indices) assert(index != -1);

			Index_set naughty_jobs;
			for (const auto &cut : cuts) {
				for (Job_index forbidden : cut.forbidden_jobs) {
					naughty_jobs.add(forbidden);
				}
				if (strategy == CUT_ENFORCEMENT_MODERN_SLOW) break;
			}
			for (Job_index naughty_job = 0; naughty_job < safe_path.size(); naughty_job++) {
				if (!naughty_jobs.contains(naughty_job)) continue;
				const size_t correct_path_index = path_indices[naughty_job];
				assert(correct_path_index > 0);
				const size_t safe_predecessor = safe_path[correct_path_index - 1];
				problem.prec.push_back(Precedence_constraint<Time>(
					problem.jobs[safe_predecessor].get_id(),
					problem.jobs[naughty_job].get_id(),
					Interval<Time>(), false
				));
			}
		} else {
			assert(strategy == CUT_ENFORCEMENT_TRADITIONAL);
			for (const Job_index forbidden : cuts[0].forbidden_jobs) {
				problem.prec.push_back(Precedence_constraint<Time>(
					problem.jobs[cuts[0].safe_job].get_id(),
					problem.jobs[forbidden].get_id(),
					Interval<Time>(), false
				));
			}
		}

		validate_prec_cstrnts<Time>(problem.prec, problem.jobs);
	}
}

#endif
