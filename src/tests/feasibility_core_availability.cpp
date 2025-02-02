#ifndef CONFIG_PARALLEL
#include "doctest.h"
#undef NDEBUG

#include "time.hpp"
#include "feasibility/core_availability.hpp"

using namespace NP::Feasibility;

TEST_CASE("Core availability with 1 core") {
	Core_availability<dtime_t> availability(1);
	REQUIRE(availability.next_start_time() == 0);
	availability.schedule(0, 4);
	REQUIRE(availability.next_start_time() == 4);
	availability.schedule(4, 5);
	REQUIRE(availability.next_start_time() == 9);
	availability.schedule(20, 3);
	REQUIRE(availability.next_start_time() == 23);
	availability.schedule(23, 1);
	CHECK(availability.next_start_time() == 24);
	CHECK(availability.number_of_processors() == 1);
}

TEST_CASE("Core availability with 3 cores") {
	Core_availability<dtime_t> availability(3);
	REQUIRE(availability.next_start_time() == 0);
	REQUIRE(availability.second_start_time() == 0);
	availability.schedule(0, 10);
	REQUIRE(availability.next_start_time() == 0);
	REQUIRE(availability.second_start_time() == 0);
	availability.schedule(0, 5);
	REQUIRE(availability.next_start_time() == 0);
	REQUIRE(availability.second_start_time() == 5);
	availability.schedule(0, 20);
	REQUIRE(availability.next_start_time() == 5);
	REQUIRE(availability.second_start_time() == 10);
	availability.schedule(7, 1);
	REQUIRE(availability.next_start_time() == 8);
	REQUIRE(availability.second_start_time() == 10);
	availability.schedule(8, 5);
	REQUIRE(availability.next_start_time() == 10);
	REQUIRE(availability.second_start_time() == 13);
	availability.schedule(10, 20);
	REQUIRE(availability.next_start_time() == 13);
	REQUIRE(availability.second_start_time() == 20);
	availability.schedule(13, 100);
	REQUIRE(availability.next_start_time() == 20);
	REQUIRE(availability.second_start_time() == 30);
	CHECK(availability.number_of_processors() == 3);
}

TEST_CASE("Core availability merging") {
	Core_availability<dtime_t> availability1(2);
	availability1.schedule(1, 2);
	availability1.schedule(0, 7);
	CHECK(availability1.number_of_processors() == 2);
	REQUIRE(availability1.next_start_time() == 3);
	REQUIRE(availability1.second_start_time() == 7);

	Core_availability<dtime_t> availability2(2);
	availability2.schedule(2, 3);
	availability2.schedule(4, 2);
	REQUIRE(availability2.next_start_time() == 5);
	REQUIRE(availability2.second_start_time() == 6);

	availability2.merge(availability1);
	CHECK(availability1.next_start_time() == 3);
	REQUIRE(availability2.next_start_time() == 5);
	REQUIRE(availability2.second_start_time() == 7);

	availability2.schedule(5, 6);
	REQUIRE(availability2.next_start_time() == 7);
	REQUIRE(availability2.second_start_time() == 11);
	availability2.schedule(7, 8);
	REQUIRE(availability2.next_start_time() == 11);
	REQUIRE(availability2.second_start_time() == 15);
	CHECK(availability2.number_of_processors() == 2);
}

TEST_CASE("Core availability get number of processors") {
	for (int num_processors = 1; num_processors < 10; num_processors += 1) {
		CHECK(Core_availability<dtime_t>(num_processors).number_of_processors() == num_processors);
	}
}

#endif
