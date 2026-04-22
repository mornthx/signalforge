// tests/unit/smoke_test.cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("smoke: Catch2 works", "[smoke]") {
    REQUIRE(1 + 1 == 2);
}

TEST_CASE("smoke: C++20 lambdas compile", "[smoke]") {
    constexpr auto sum = [](auto a, auto b) { return a + b; };
    STATIC_REQUIRE(sum(1, 2) == 3);
}
