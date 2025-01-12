#ifndef RECONFIGURATION_CUT_ENFORCER_H
#define RECONFIGURATION_CUT_ENFORCER_H

#include "rating_graph.hpp"
#include "problem.hpp"
#include "feasibility/simple_bounds.hpp"

namespace NP::Reconfiguration {
    template<class Time> static void enforce_cuts(
        NP::Scheduling_problem<Time> &problem, size_t num_original_constraints,
        std::vector<Rating_graph_cut> &cuts, const Feasibility::Simple_bounds<Time> &feasibility_bounds
    ) {
        for (auto &cut : cuts) {
            std::sort(cut.safe_jobs.begin(), cut.safe_jobs.end(), [feasibility_bounds](const Job_index &a, const Job_index &b) {
                return feasibility_bounds.earliest_pessimistic_start_times[a] < feasibility_bounds.earliest_pessimistic_start_times[b];
            });

            const Job_index safe_job_index = cut.safe_jobs[0];
            for (const Job_index forbidden_job_index : cut.forbidden_jobs) {
                bool is_already_enforced = false;
                for (size_t existing_index = num_original_constraints; existing_index < problem.prec.size(); existing_index++) {
                    if (problem.prec[existing_index].get_fromIndex() == safe_job_index && problem.prec[existing_index].get_toIndex() == forbidden_job_index) {
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
