#ifndef CONFIG_PARALLEL
#include "doctest.h"
#undef NDEBUG

#include "problem.hpp"
#include "feasibility/packing.hpp"

using namespace NP;
using namespace NP::Feasibility;

TEST_CASE("Feasibility packing without jobs") {
	std::vector<dtime_t> empty{};
	CHECK(!is_certainly_unpackable<dtime_t>(1, 10, empty));
	CHECK(!is_certainly_unpackable<dtime_t>(2, 10, empty));
	CHECK(!is_certainly_unpackable<dtime_t>(5, 10, empty));

	CHECK(!is_certainly_unpackable<dtime_t>(1, 0, empty));
	CHECK(!is_certainly_unpackable<dtime_t>(2, 0, empty));
	CHECK(!is_certainly_unpackable<dtime_t>(5, 0, empty));
}

TEST_CASE("Feasibility packing with 1 job") {
	std::vector<dtime_t> job{100};
	CHECK(is_certainly_unpackable<dtime_t>(1, 99, job));
	CHECK(is_certainly_unpackable<dtime_t>(5, 99, job));
	CHECK(!is_certainly_unpackable<dtime_t>(1, 100, job));
	CHECK(!is_certainly_unpackable<dtime_t>(5, 100, job));
}

TEST_CASE("Feasibility packing with 2 equally long jobs") {
	std::vector<dtime_t> jobs{100, 100};
	CHECK(is_certainly_unpackable<dtime_t>(1, 99, jobs));
	CHECK(is_certainly_unpackable<dtime_t>(2, 99, jobs));
	CHECK(is_certainly_unpackable<dtime_t>(5, 99, jobs));

	CHECK(is_certainly_unpackable<dtime_t>(1, 100, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(2, 100, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(5, 100, jobs));

	CHECK(is_certainly_unpackable<dtime_t>(1, 197, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(1, 200, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(1, 300, jobs));
}

TEST_CASE("Feasibility packing with 2 jobs of different length") {
	std::vector<dtime_t> jobs{100, 50};
	CHECK(is_certainly_unpackable<dtime_t>(1, 99, jobs));
	CHECK(is_certainly_unpackable<dtime_t>(2, 99, jobs));
	CHECK(is_certainly_unpackable<dtime_t>(5, 99, jobs));

	CHECK(is_certainly_unpackable<dtime_t>(1, 100, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(2, 100, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(5, 100, jobs));

	CHECK(is_certainly_unpackable<dtime_t>(1, 149, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(1, 150, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(1, 197, jobs));
}

TEST_CASE("Feasibility packing with 3 equally long jobs") {
	std::vector<dtime_t> jobs{100, 100, 100};
	CHECK(is_certainly_unpackable<dtime_t>(1, 99, jobs));
	CHECK(is_certainly_unpackable<dtime_t>(3, 99, jobs));
	CHECK(is_certainly_unpackable<dtime_t>(100, 99, jobs));

	CHECK(is_certainly_unpackable<dtime_t>(1, 100, jobs));
	CHECK(is_certainly_unpackable<dtime_t>(2, 100, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(3, 100, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(4, 100, jobs));

	CHECK(is_certainly_unpackable<dtime_t>(1, 299, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(1, 300, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(1, 301, jobs));

	CHECK(is_certainly_unpackable<dtime_t>(2, 199, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(2, 200, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(2, 299, jobs));
}

TEST_CASE("Feasibility packing with 3 jobs of different length") {
	std::vector<dtime_t> jobs{100, 50, 60};
	CHECK(is_certainly_unpackable<dtime_t>(1, 209, jobs));
	CHECK(is_certainly_unpackable<dtime_t>(2, 109, jobs));
	CHECK(is_certainly_unpackable<dtime_t>(5, 99, jobs));

	CHECK(is_certainly_unpackable<dtime_t>(1, 110, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(2, 110, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(5, 110, jobs));

	CHECK(!is_certainly_unpackable<dtime_t>(1, 210, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(2, 210, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(3, 100, jobs));
}

TEST_CASE("Feasibility packing with 4 equally long jobs") {
	std::vector<dtime_t> jobs{100, 100, 100, 100};
	CHECK(is_certainly_unpackable<dtime_t>(1, 99, jobs));
	CHECK(is_certainly_unpackable<dtime_t>(4, 99, jobs));
	CHECK(is_certainly_unpackable<dtime_t>(100, 99, jobs));

	CHECK(is_certainly_unpackable<dtime_t>(1, 100, jobs));
	CHECK(is_certainly_unpackable<dtime_t>(3, 100, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(4, 100, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(123, 100, jobs));

	CHECK(is_certainly_unpackable<dtime_t>(1, 399, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(1, 400, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(1, 401, jobs));

	CHECK(is_certainly_unpackable<dtime_t>(2, 199, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(2, 200, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(2, 399, jobs));
}

TEST_CASE("Feasibility packing with 4 jobs of different length") {
	std::vector<dtime_t> jobs{100, 50, 80, 20};
	CHECK(is_certainly_unpackable<dtime_t>(1, 249, jobs));
	CHECK(is_certainly_unpackable<dtime_t>(2, 129, jobs));
	CHECK(is_certainly_unpackable<dtime_t>(4, 99, jobs));
	CHECK(is_certainly_unpackable<dtime_t>(9, 99, jobs));

	CHECK(!is_certainly_unpackable<dtime_t>(1, 250, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(2, 130, jobs));
	CHECK(!is_certainly_unpackable<dtime_t>(5, 130, jobs));

	CHECK(!is_certainly_unpackable<dtime_t>(3, 100, jobs));
}

#endif
