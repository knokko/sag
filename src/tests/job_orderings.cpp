#ifndef CONFIG_PARALLEL
#include "doctest.h"
#undef NDEBUG

#include "problem.hpp"
#include "reconfiguration/job_orderings.hpp"
#include "reconfiguration/graph_cutter.hpp"
#include "feasibility/graph.hpp"

using namespace NP;
using namespace NP::Reconfiguration;
using namespace NP::Feasibility;

TEST_CASE("Reconfiguration job orderings") {
	Job_orderings orderings(10);

	orderings.mark_dispatchable(3);
	orderings.mark_dispatchable(5);

	orderings.advance();
	orderings.mark_dispatchable(2);
	orderings.mark_dispatchable(5);

	orderings.advance();
	orderings.mark_dispatchable(2);
	orderings.mark_dispatchable(3);

	orderings.finalize();

	CHECK(orderings.get_earliest_occurrence_index(2) == 0);
	CHECK(orderings.get_earliest_occurrence_index(3) == 0);
	CHECK(orderings.get_earliest_occurrence_index(5) == 1);

	CHECK(orderings.get_latest_occurrence_index(2) == 1);
	CHECK(orderings.get_latest_occurrence_index(3) == 2);
	CHECK(orderings.get_latest_occurrence_index(5) == 2);

	for (NP::Job_index job_index = 0; job_index < 10; job_index++) {
		if (job_index < 2 || job_index == 4 || job_index > 6) {
			CHECK(orderings.get_earliest_occurrence_index(job_index) == 3);
			CHECK(orderings.get_latest_occurrence_index(job_index) == 9);
		}
	}
}

TEST_CASE("determine_job_orderings_for_cuts on rating graph example") {
	Global::State_space<dtime_t>::Workload jobs{
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

	auto problem = Scheduling_problem<dtime_t>(jobs);
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	const auto bounds = compute_simple_bounds(problem);

	Rating_graph rating_graph;
	Agent_rating_graph<dtime_t>::generate(problem, rating_graph);

	Feasibility_graph<dtime_t> feasibility_graph(rating_graph);
	feasibility_graph.explore_forward(problem, bounds, predecessor_mapping);
	feasibility_graph.explore_backward();

	const auto cuts = cut_rating_graph(rating_graph, feasibility_graph);
	const auto cut_orderings = determine_orderings_for_cuts(jobs.size(), rating_graph, cuts);
	REQUIRE(cut_orderings.size() == 1);

	const auto &job_orderings = cut_orderings[0];
	CHECK(job_orderings.get_earliest_occurrence_index(0) == 0);
	CHECK(job_orderings.get_latest_occurrence_index(0) == 0);
	CHECK(job_orderings.get_earliest_occurrence_index(6) == 1);
	CHECK(job_orderings.get_latest_occurrence_index(6) == 1);

	for (size_t job_index = 1; job_index < jobs.size(); job_index++) {
		if (job_index == 6) continue;
		CHECK(job_orderings.get_earliest_occurrence_index(job_index) == 2);
		CHECK(job_orderings.get_latest_occurrence_index(job_index) == jobs.size() - 1);
	}
}

#endif
