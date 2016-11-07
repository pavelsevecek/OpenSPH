#include "storage/Quantity.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Quantity", "[quantity]") {
    Quantity q1;
    q1.emplace<Float, TemporalType::FIRST_ORDER>();
    using namespace QuantityCast;
    REQUIRE(get<Float>(q1));
    REQUIRE(dt<Float>(q1));
    REQUIRE_FALSE(dt2<Float>(q1));

    auto opt = q1.cast<Float, TemporalType::FIRST_ORDER>();
    REQUIRE(opt);
    REQUIRE(opt->getBuffers().size() == 2);
}
