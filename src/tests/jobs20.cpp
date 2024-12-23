#ifndef CONFIG_PARALLEL
#include "doctest.h"
#undef NDEBUG

#include <iostream>
#include <sstream>

#include "io.hpp"
#include "global/space.hpp"
#include "reconfiguration/graph_strategy.hpp"

const std::string jobs20 =
		"Task ID,Job ID,Arrival min,Arrival max,Cost min,Cost max,Deadline,Priority\n"
		"1,1,0,500,104,209,2000,2\n"
		"1,2,0,500,14,29,2000,2\n"
		"1,3,0,500,94,188,2000,2\n"
		"1,4,5000,5500,104,209,10000,1\n"
		"1,5,5000,5500,14,29,10000,1\n"
		"1,6,5000,5500,94,188,10000,1\n"
		"1,7,10000,10500,104,209,15000,1\n"
		"1,8,10000,10500,14,29,15000,1\n"
		"1,9,10000,10500,94,188,15000,1\n"
		"1,10,15000,15500,104,209,20000,1\n"
		"1,11,15000,15500,14,29,20000,1\n"
		"1,12,15000,15500,94,188,20000,1\n"
		"2,1,0,1000,39,79,10000,2\n"
		"2,2,0,1000,46,92,10000,2\n"
		"2,3,0,1000,23,46,10000,2\n"
		"2,4,10000,11000,39,79,20000,2\n"
		"2,5,10000,11000,46,92,20000,2\n"
		"2,6,10000,11000,23,46,20000,2\n"
		"3,1,0,1000,457,914,10000,3\n"
		"3,2,0,1000,229,459,10000,3\n"
		"3,3,0,1000,207,414,10000,3\n"
		"3,4,10000,11000,457,914,20000,3\n"
		"3,5,10000,11000,229,459,20000,3\n"
		"3,6,10000,11000,207,414,20000,3\n"
		"4,1,0,1000,908,1817,20000,1\n"
		"4,2,0,2000,343,687,20000,1\n"
		"4,3,0,2000,57,114,20000,4\n"
		"5,1,0,2000,246,492,20000,1\n"
		"5,2,0,2000,531,1062,20000,1\n"
		"5,3,0,2000,51,102,20000,5";

const std::string jobs21 =
		"Task ID,Job ID,Arrival min,Arrival max,Cost min,Cost max,Deadline,Priority\n"
		"1,1,0,501,104,209,2000,1\n"
		"1,2,0,502,14,29,2000,0\n"
		"1,3,0,503,94,188,2000,0\n"
		"1,4,5000,5500,104,209,10000,1\n"
		"1,5,5000,5500,14,29,10000,1\n"
		"1,6,5000,5500,94,188,10000,1\n"
		"1,7,10000,10500,104,209,15000,1\n"
		"1,8,10000,10500,14,29,15000,1\n"
		"1,9,10000,10500,94,188,15000,1\n"
		"1,10,15000,15500,104,209,20000,1\n"
		"1,11,15000,15500,14,29,20000,1\n"
		"1,12,15000,15500,94,188,20000,1\n"
		"2,1,600,1000,39,79,10000,2\n"
		"2,2,600,1000,46,92,10000,2\n"
		"2,3,600,1000,23,46,10000,2\n"
		"2,4,10000,11000,39,79,20000,2\n"
		"2,5,10000,11000,46,92,20000,2\n"
		"2,6,10000,11000,23,46,20000,2\n"
		"3,1,900,1000,457,914,10000,3\n"
		"3,2,900,1000,229,459,10000,3\n"
		"3,3,900,1000,207,414,10000,3\n"
		"3,4,10000,11000,457,914,20000,3\n"
		"3,5,10000,11000,229,459,20000,3\n"
		"3,6,10000,11000,207,414,20000,3\n"
		"4,1,0,2000,908,1817,20000,4\n"
		"4,2,0,2000,343,687,20000,4\n"
		"4,3,0,2000,57,114,20000,4\n"
		"5,1,0,2000,246,492,20000,1\n"
		"5,2,0,2000,531,1062,20000,1\n"
		"5,3,0,2000,51,102,20000,5";

using namespace NP;

TEST_CASE("Graph strategy on a really nasty graph") {
	auto in = std::istringstream(jobs21);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	const auto problem = Scheduling_problem<dtime_t>(jobs, std::vector<Precedence_constraint<dtime_t>>());

	Reconfiguration::Rating_graph rating_graph;
	Reconfiguration::Agent_rating_graph<dtime_t>::generate(problem, rating_graph);

	auto cuts = Reconfiguration::cut_rating_graph(rating_graph);
	rating_graph.generate_dot_file("test_nasty_rating_graph_with_cuts.dot", problem, cuts);

	const auto solutions = Reconfiguration::apply_graph_strategy<dtime_t>(problem);
	for (const auto &solution : solutions) solution->print();
	//REQUIRE(solutions.size() == 1); TODO Finish this
}
#endif