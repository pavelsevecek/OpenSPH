#include "catch.hpp"
#include "sph/solvers/ContinuitySolver.h"
#include "tests/Setup.h"

using namespace Sph;


TEST_CASE("Impact", "[impact]]") {
    // Check that first two steps of impact work as expected, specifically:
    // 1. After first step, the StrengthVelocityGradient should be zero, meaning derivatives of stress tensor
    //    and density should be zero as well
    // 2. Derivatives are nonzero in the second step (as there is already a nonzero velocity gradient inside
    //    each body)

    /// \todo
}
