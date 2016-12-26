#include "sph/timestepping/TimeStepping.h"
#include "catch.hpp"
#include "solvers/AbstractSolver.h"
#include "storage/QuantityMap.h"
#include "storage/Storage.h"
#include <iostream>

using namespace Sph;

struct TestSolver : public Abstract::Solver {
    Float period = 1._f;

    TestSolver() = default;

    /* virtual void setQuantities(Storage&,
          const Abstract::Domain&,
          const Settings<BodySettingsIds>&) const override {
          NOT_IMPLEMENTED;}*/

    virtual void integrate(Storage& storage) override {
        ArrayView<Vector> r, v, dv;
        tieToArray(r, v, dv) = storage.getAll<Vector>(QuantityKey::POSITIONS);
        Float omega = 2._f * Math::PI / period;
        for (int i = 0; i < r.size(); ++i) {
            dv[i] = -Math::sqr(omega) * r[i];
        }
    }

    virtual void initialize(Storage&, const BodySettings&) const override { NOT_IMPLEMENTED; }
};

const Float timeStep = 0.01_f;

template <typename TTimestepping, typename... TArgs>
void testTimestepping(TArgs&&... args) {
    TestSolver solver;

    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    storage->emplace<Vector, OrderEnum::SECOND_ORDER>(
        QuantityKey::POSITIONS, Array<Vector>{ Vector(1._f, 0._f, 0._f) });

    ArrayView<const Vector> r, v, dv;
    tieToArray(r, v, dv) = storage->getAll<Vector>(QuantityKey::POSITIONS);
    TTimestepping timestepping(storage, std::forward<TArgs>(args)...);
    int n;
    for (float t = 0.f; t < 3.f; t += timestepping.getTimeStep()) {
        // std::cout << rs[0][X] << "  " << vs[0][X] << "  " << Math::cos(2.f * Math::PI * t) << std::endl;
        if ((n++ % 15) == 0) {
            REQUIRE(Math::almostEqual(
                r[0], Vector(Math::cos(2.f * Math::PI * t), 0.f, 0.f), timeStep * 2._f * Math::PI));
            REQUIRE(Math::almostEqual(v[0],
                Vector(-Math::sin(2.f * Math::PI * t) * 2.f * Math::PI, 0.f, 0.f),
                timeStep * Math::sqr(2._f * Math::PI)));
        }
        timestepping.step(solver);
    }
}

TEST_CASE("harmonic oscilator", "[timestepping]") {
    Settings<GlobalSettingsIds> settings(GLOBAL_SETTINGS);
    settings.set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, timeStep);

    testTimestepping<EulerExplicit>(settings);
    testTimestepping<PredictorCorrector>(settings);
}
