#include "sph/equations/Derivative.h"
#include "catch.hpp"
#include "objects/utility/PerElementWrapper.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Derivative require", "[derivative]") {
    DerivativeHolder derivatives;
    RunSettings settings;
    REQUIRE(derivatives.getDerivativeCnt() == 0);
    derivatives.require(makeDerivative<VelocityDivergence>(settings));
    REQUIRE(derivatives.getDerivativeCnt() == 1);
    // same type but different state -> ignore anyway
    derivatives.require(makeDerivative<VelocityDivergence>(settings, DerivativeFlag::SUM_ONLY_UNDAMAGED));
    REQUIRE(derivatives.getDerivativeCnt() == 1);
    derivatives.require(makeDerivative<VelocityGradient>(settings));
    REQUIRE(derivatives.getDerivativeCnt() == 2);
}

TEST_CASE("Derivative initialize", "[derivative]") {
    DerivativeHolder derivatives;
    RunSettings settings;
    derivatives.require(makeDerivative<VelocityDivergence>(settings));
    Storage storage;
    storage.insert<Vector>(
        QuantityId::POSITION, OrderEnum::FIRST, Array<Vector>{ Vector(1._f), Vector(2._f), Vector(3._f) });
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, 1._f); // quantities needed by divv
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, 1._f);

    derivatives.initialize(storage);
    Accumulated& ac = derivatives.getAccumulated();
    REQUIRE(ac.getBufferCnt() == 1);
    ArrayView<Float> divv = ac.getBuffer<Float>(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO);
    REQUIRE(divv.size() == 3);
    REQUIRE(perElement(divv) == 0._f);
}

template <int I, BufferSource Source>
class DummyDerivative : public IDerivative {
public:
    virtual void create(Accumulated& results) override {
        results.insert<Float>(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO, Source);
    }

    virtual void initialize(const Storage& UNUSED(input), Accumulated& UNUSED(results)) override {}

    virtual void evalNeighs(const Size UNUSED(idx),
        ArrayView<const Size> UNUSED(neighs),
        ArrayView<const Vector> UNUSED(grads)) override {}

    virtual DerivativePhase phase() const override {
        return DerivativePhase::EVALUATION;
    }
};

TEST_CASE("Derivative unique buffer", "[derivative]") {
    DerivativeHolder derivatives;
    RunSettings settings;
    derivatives.require(makeAuto<DummyDerivative<1, BufferSource::UNIQUE>>());
    derivatives.require(makeAuto<DummyDerivative<2, BufferSource::UNIQUE>>());

    Storage storage;
    storage.insert<Float>(QuantityId::POSITION, OrderEnum::FIRST, Array<Float>{ 1._f, 2._f, 3._f });
    REQUIRE_ASSERT(derivatives.initialize(storage));
}

TEST_CASE("Derivative shared buffer", "[derivative]") {
    DerivativeHolder derivatives;
    RunSettings settings;
    derivatives.require(makeAuto<DummyDerivative<1, BufferSource::SHARED>>());
    derivatives.require(makeAuto<DummyDerivative<2, BufferSource::SHARED>>());

    Storage storage;
    storage.insert<Float>(QuantityId::POSITION, OrderEnum::FIRST, Array<Float>{ 1._f, 2._f, 3._f });
    REQUIRE_NOTHROW(derivatives.initialize(storage));
}

TEST_CASE("Derivative isSymmetric", "[derivative]") {
    DerivativeHolder derivatives;
    RunSettings settings;
    derivatives.require(makeDerivative<VelocityDivergence>(settings));
    REQUIRE(derivatives.isSymmetric());

    derivatives.require(makeDerivative<VelocityGradient>(settings));
    REQUIRE(derivatives.isSymmetric());

    class AsymmetricDerivative : public IDerivative {
        virtual DerivativePhase phase() const override {
            return DerivativePhase::EVALUATION;
        }

        virtual void create(Accumulated& UNUSED(results)) override {}

        virtual void initialize(const Storage& UNUSED(input), Accumulated& UNUSED(results)) override {}

        virtual void evalNeighs(const Size UNUSED(idx),
            ArrayView<const Size> UNUSED(neighs),
            ArrayView<const Vector> UNUSED(grads)) override {}
    };
    derivatives.require(makeAuto<AsymmetricDerivative>());
    REQUIRE_FALSE(derivatives.isSymmetric());
}
