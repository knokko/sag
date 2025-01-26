#ifndef RECONFIGURATION_REMOVE_CONSTRAINTS_H
#define RECONFIGURATION_REMOVE_CONSTRAINTS_H

#include "problem.hpp"

namespace NP::Reconfiguration {

	template<class Time> static void remove_constraints(Scheduling_problem<Time> &problem, std::vector<size_t> constraint_indices_to_remove) {
		std::sort(constraint_indices_to_remove.begin(), constraint_indices_to_remove.end(), std::greater<>());
		for (const size_t constraint_index : constraint_indices_to_remove) {
			problem.prec[constraint_index] = problem.prec[problem.prec.size() - 1];
			problem.prec.pop_back();
		}
		validate_prec_cstrnts<Time>(problem.prec, problem.jobs);
	}
}

#endif
