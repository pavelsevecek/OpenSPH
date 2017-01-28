#include "utils/SequenceTest.h"
#include "catch.hpp"

using namespace Sph;

/// Meta-tests, we are testing our extension for testing sequences of tests

TEST_CASE("Sequence success", "[sequencetest]") {
    Size testsPerformed = 0;
    auto test = [&](const Size i) {
        testsPerformed++;
        return i > 20;
    };

    REQUIRE_SEQUENCE(test, 30, 50);
    REQUIRE(testsPerformed == 20);
}

TEST_CASE("Sequence fail", "[sequencetests]") {
    auto test = [&](const Size i) {
        if (i > 50 && i < 60) {
            return makeFailed("e", i);
        }
        if (i >= 60) {
            return makeFailed("f", i);
        }
        return SUCCESS;
    };

    auto seq1 = testSequence(test, 0, 50);
    REQUIRE(seq1.outcome());

    auto seq2 = testSequence(test, 0, 55);
    REQUIRE_FALSE(seq2.outcome());
    REQUIRE(seq2.outcome().error() == "e51");

    auto seq3 = testSequence(test, 65, 90);
    REQUIRE_FALSE(seq3.outcome());
    REQUIRE(seq3.outcome().error() == "f65");
}
