#include "sph/timestepping/TimeStepping.h"
#include "catch.hpp"
#include "quantities/Storage.h"
#include "solvers/AbstractSolver.h"

using namespace Sph;

struct TestSolver : public Abstract::Solver {
    Float period = 1._f;

    TestSolver() = default;

    virtual void integrate(Storage& storage) override {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityKey::POSITIONS);
        Float omega = 2._f * PI / period;
        for (Size i = 0; i < r.size(); ++i) {
            dv[i] = -sqr(omega) * r[i];
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
    tie(r, v, dv) = storage->getAll<Vector>(QuantityKey::POSITIONS);
    TTimestepping timestepping(storage, std::forward<TArgs>(args)...);
    FrequentStats stats;
    Size n = 0;
    for (float t = 0.f; t < 3.f; t += timestepping.getTimeStep()) {
        if ((n++ % 15) == 0) {
            REQUIRE(almostEqual(
                r[0], Vector(cos(2.f * PI * t), 0.f, 0.f), timeStep * 2._f * PI));
            REQUIRE(almostEqual(v[0],
                Vector(-sin(2.f * PI * t) * 2.f * PI, 0.f, 0.f),
                timeStep * sqr(2._f * PI)));
        }
        timestepping.step(solver, stats);
    }
}

TEST_CASE("Timestepping Harmonic oscilator", "[timestepping]") {
    Settings<GlobalSettingsIds> settings(GLOBAL_SETTINGS);
    settings.set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, timeStep);

    testTimestepping<EulerExplicit>(settings);
    testTimestepping<PredictorCorrector>(settings);
}
