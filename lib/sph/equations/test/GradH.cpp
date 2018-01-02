#include "sph/equations/GradH.h"
#include "catch.hpp"
#include "sph/solvers/AsymmetricSolver.h"
#include "tests/Setup.h"
#include "utils/SequenceTest.h"

using namespace Sph;

TEST_CASE("GradH", "[solvers]") {
    Storage storage = Tests::getGassStorage(1000, BodySettings::getDefaults());
    storage.insert<Float>(QuantityId::GRAD_H, OrderEnum::ZERO, 0._f);
    REQUIRE_NOTHROW((Tests::computeField<GradH, AsymmetricSolver>(
        storage, [](Vector UNUSED(r)) { return Vector(0._f); })));

    // check that it computed 'something'
    ArrayView<const Float> omega = storage.getValue<Float>(QuantityId::GRAD_H);
    auto test = [omega](Size i) -> Outcome { return Outcome(omega[i] > 0._f); };
    REQUIRE_SEQUENCE(test, 0, omega.size());
}
