#ifndef CONFIG_PARALLEL
#include "doctest.h"
#undef NDEBUG

#include "problem.hpp"
#include "reconfiguration/tail_constraint_minimizer.hpp"

using namespace NP;
using namespace NP::Reconfiguration;

TEST_CASE("Tail constraint minimizer on simple rating graph problem") {
	std::vector<Job<dtime_t>> jobs{
			// high-frequency task
			Job<dtime_t>{0, Interval<dtime_t>(0,  0), Interval<dtime_t>(1, 2), 10, 10, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(10, 10), Interval<dtime_t>(1, 2), 20, 20, 1, 1},
			Job<dtime_t>{2, Interval<dtime_t>(20, 20), Interval<dtime_t>(1, 2), 30, 30, 2, 2},
			Job<dtime_t>{3, Interval<dtime_t>(30, 30), Interval<dtime_t>(1, 2), 40, 40, 3, 3},
			Job<dtime_t>{4, Interval<dtime_t>(40, 40), Interval<dtime_t>(1, 2), 50, 50, 4, 4},
			Job<dtime_t>{5, Interval<dtime_t>(50, 50), Interval<dtime_t>(1, 2), 60, 60, 5, 5},

			// middle task
			Job<dtime_t>{6, Interval<dtime_t>(0,  0), Interval<dtime_t>(7, 8), 30, 30, 6, 6},
			Job<dtime_t>{7, Interval<dtime_t>(30, 30), Interval<dtime_t>(7, 8), 60, 60, 7, 7},

			// the long task
			Job<dtime_t>{8, Interval<dtime_t>(0,  0), Interval<dtime_t>(3, 13), 60, 60, 8, 8}
	};
	std::vector<Precedence_constraint<dtime_t>> prec{
		{jobs[6].get_id(), jobs[7].get_id(), Interval<dtime_t>()}, // Useless constraint
		{jobs[0].get_id(), jobs[6].get_id(), Interval<dtime_t>()}, // Useless constraint
		{jobs[1].get_id(), jobs[8].get_id(), Interval<dtime_t>()}, // Crucial constraint
		{jobs[0].get_id(), jobs[1].get_id(), Interval<dtime_t>()}, // Useless constraint
	};

	{
		Scheduling_problem<dtime_t> problem(jobs, prec);
		auto minimizer = Tail_constraint_minimizer<dtime_t>(problem, 1);
		CHECK(!minimizer.can_remove(3));
		CHECK(!minimizer.can_remove(2));
		CHECK(minimizer.can_remove(1));

		CHECK(minimizer.try_to_remove(2) == 1);
		CHECK(problem.prec.size() == 3);

		CHECK(!minimizer.can_remove(1));
		CHECK(minimizer.try_to_remove(1) == 0);

		CHECK(minimizer.can_remove(1));
		CHECK(minimizer.try_to_remove(1) == 1);

		REQUIRE(problem.prec.size() == 2);
		CHECK(problem.prec[0].get_fromIndex() == 6);
		CHECK(problem.prec[0].get_toIndex() == 7);
		CHECK(problem.prec[1].get_fromIndex() == 1);
		CHECK(problem.prec[1].get_toIndex() == 8);
	}

	{
		Scheduling_problem<dtime_t> problem(jobs, prec);
		REQUIRE(problem.prec.size() == 4); // Sanity check
		auto minimizer = Tail_constraint_minimizer<dtime_t>(problem, 1);
		minimizer.remove_constraints_until_finished(4, false);

		REQUIRE(problem.prec.size() == 2);
		CHECK(problem.prec[0].get_fromIndex() == 6);
		CHECK(problem.prec[0].get_toIndex() == 7);
		CHECK(problem.prec[1].get_fromIndex() == 1);
		CHECK(problem.prec[1].get_toIndex() == 8);
	}

	{
		Scheduling_problem<dtime_t> problem(jobs, prec);
		REQUIRE(problem.prec.size() == 4); // Sanity check
		auto minimizer = Tail_constraint_minimizer<dtime_t>(problem, 1);
		minimizer.remove_constraints_until_finished(1, false);

		REQUIRE(problem.prec.size() == 2);
		CHECK(problem.prec[0].get_fromIndex() == 6);
		CHECK(problem.prec[0].get_toIndex() == 7);
		CHECK(problem.prec[1].get_fromIndex() == 1);
		CHECK(problem.prec[1].get_toIndex() == 8);
	}
}

#endif
