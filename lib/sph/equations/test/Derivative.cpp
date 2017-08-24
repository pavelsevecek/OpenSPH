#include "sph/equations/Derivative.h"
#include "catch.hpp"
#include "objects/utility/PerElementWrapper.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Derivative require", "[derivative]") {
    DerivativeHolder derivatives;
    RunSettings settings;
    REQUIRE(derivatives.getDerivativeCnt() == 0);
    derivatives.require<VelocityDivergence>(settings);
    REQUIRE(derivatives.getDerivativeCnt() == 1);
    derivatives.require<VelocityDivergence>(settings);
    REQUIRE(derivatives.getDerivativeCnt() == 1);
}

TEST_CASE("Derivative initialize", "[derivative]") {
    DerivativeHolder derivatives;
    RunSettings settings;
    derivatives.require<VelocityDivergence>(settings);
    Storage storage;
    storage.insert<Vector>(
        QuantityId::POSITIONS, OrderEnum::FIRST, Array<Vector>{ Vector(1._f), Vector(2._f), Vector(3._f) });
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, 1._f); // quantities needed by divv
    storage.insert<Float>(QuantityId::MASSES, OrderEnum::ZERO, 1._f);

    derivatives.initialize(storage);
    Accumulated& ac = derivatives.getAccumulated();
    REQUIRE(ac.getBufferCnt() == 1);
    ArrayView<Float> divv = ac.getBuffer<Float>(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO);
    REQUIRE(divv.size() == 3);
    REQUIRE(perElement(divv) == 0._f);
}
