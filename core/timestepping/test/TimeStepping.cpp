#include "timestepping/TimeStepping.h"
#include "catch.hpp"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "sph/Materials.h"
#include "system/Settings.h"
#include "system/Statistics.h"
#include "tests/Approx.h"
#include "tests/Setup.h"
#include "timestepping/ISolver.h"
#include "utils/SequenceTest.h"

using namespace Sph;

namespace {
struct HomogeneousField : public ISolver {
    Vector g = Vector(0.f, 0.f, 1._f);

    HomogeneousField() = default;

    virtual void integrate(Storage& storage, Statistics&) override {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
        for (Size i = 0; i < r.size(); ++i) {
            dv[i] = g;
        }
    }

    virtual void create(Storage&, IMaterial&) const override {
        NOT_IMPLEMENTED;
    }
};

struct HarmonicOscillator : public ISolver {
    Float period = 1._f;

    HarmonicOscillator() = default;

    virtual void integrate(Storage& storage, Statistics&) override {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
        Float omega = 2._f * PI / period;
        for (Size i = 0; i < r.size(); ++i) {
            dv[i] = -sqr(omega) * r[i];
        }
    }

    virtual void create(Storage&, IMaterial&) const override {
        NOT_IMPLEMENTED;
    }
};

struct LorentzForce : public ISolver {
    const Vector B = Vector(0.f, 0.f, 1.f);

    LorentzForce() = default;

    virtual void integrate(Storage& storage, Statistics&) override {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
        for (Size i = 0; i < r.size(); ++i) {
            dv[i] = cross(v[i], B);
        }
    }

    virtual void create(Storage&, IMaterial&) const override {
        NOT_IMPLEMENTED;
    }
};

} // namespace

const Float timeStep = 0.01_f;


/// \todo all three tests test the same thing. Add more complex tests, or at least something with more
/// particles,

template <typename TTimestepping, typename... TArgs>
static void testHomogeneousField(TArgs&&... args) {
    HomogeneousField solver;

    SharedPtr<Storage> storage = makeShared<Storage>(getMaterial(MaterialEnum::BASALT));
    storage->insert<Vector>(
        QuantityId::POSITION, OrderEnum::SECOND, Array<Vector>{ Vector(0._f, 0._f, 0._f) });

    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
    TTimestepping timestepping(storage, std::forward<TArgs>(args)...);
    Statistics stats;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();

    const Size testCnt = Size(3._f / timeStep);
    auto test = [&](const Size i) -> Outcome {
        const Float t = i * timeStep;
        const Vector pos(0.f, 0.f, 0.5 * sqr(t));
        const Vector vel(0.f, 0.f, t);
        if (r[0] != approx(pos, 2.f * timeStep)) {
            return makeFailed("Invalid position: \n{} == {}\n t == {}", r[0], pos, t);
        }
        if (v[0] != approx(vel, timeStep)) {
            return makeFailed("Invalid velocity: \n{} == {}\n t == {}", v[0], vel, t);
        }
        timestepping.step(pool, solver, stats);
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, testCnt);
}

template <typename TTimestepping, typename... TArgs>
static void testHarmonicOscillator(TArgs&&... args) {
    HarmonicOscillator solver;

    SharedPtr<Storage> storage = makeShared<Storage>(getMaterial(MaterialEnum::BASALT));
    storage->insert<Vector>(
        QuantityId::POSITION, OrderEnum::SECOND, Array<Vector>{ Vector(1._f, 0._f, 0._f) });

    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
    TTimestepping timestepping(storage, std::forward<TArgs>(args)...);
    Statistics stats;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();

    const Size testCnt = Size(3._f / timeStep);
    auto test = [&](const Size i) -> Outcome {
        const Float t = i * timeStep;
        if (r[0] != approx(Vector(Sph::cos(2.f * PI * t), 0.f, 0.f), timeStep * 2._f * PI)) {
            return makeFailed(
                "Invalid position: \n{} == {}\nt == {}", r[0], Vector(Sph::cos(2.f * PI * t), 0.f, 0.f), t);
        }
        if (v[0] != approx(Vector(-Sph::sin(2.f * PI * t) * 2.f * PI, 0.f, 0.f), timeStep * sqr(2._f * PI))) {
            return makeFailed(
                "Invalid velocity: \n{} == {}\nt == {}", v[0], Vector(-Sph::sin(2.f * PI * t), 0.f, 0.f), t);
        }
        timestepping.step(pool, solver, stats);
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, testCnt);
}

template <typename TTimestepping, typename... TArgs>
static void testGyroscopicMotion(TArgs&&... args) {
    LorentzForce solver;

    SharedPtr<Storage> storage = makeShared<Storage>(getMaterial(MaterialEnum::BASALT));
    storage->insert<Vector>(
        QuantityId::POSITION, OrderEnum::SECOND, Array<Vector>{ Vector(1._f, 0._f, 0._f) });

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
    v[0] = Vector(0._f, -1._f, 0.5_f);
    // y component is perpendicular, should oscilate with cyclotron frequency, in this case omega_C = B
    // z component is parallel, should move with constant velocity

    TTimestepping timestepping(storage, std::forward<TArgs>(args)...);
    Statistics stats;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();

    const Size testCnt = Size(3._f / timeStep);
    auto test = [&](const Size i) -> Outcome {
        const Float t = i * timeStep;
        const Vector pos = Vector(Sph::cos(t), -Sph::sin(t), 0.5_f * t);
        const Vector vel = Vector(-Sph::sin(t), -Sph::cos(t), 0.5_f);
        if (r[0] != approx(pos, 3.f * timeStep)) {
            return makeFailed("Invalid position: \n{} == {}\n t == {}", r[0], pos, t);
        }
        if (v[0] != approx(vel, 3._f * timeStep)) {
            return makeFailed("Invalid velocity: \n{} == {}\n t == {}", v[0], vel, t);
        }
        timestepping.step(pool, solver, stats);
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, testCnt);
}

struct ClampSolver : public ISolver {
    enum class Direction {
        INCREASING,
        DECREASING,
    } direction;

    Interval range;

    ClampSolver(const Direction direction, const Interval range)
        : direction(direction)
        , range(range) {}

    virtual void integrate(Storage& storage, Statistics&) override {
        ArrayView<Float> u, du;
        tie(u, du) = storage.getAll<Float>(QuantityId::ENERGY);
        for (Size i = 0; i < du.size(); ++i) {
            if (direction == Direction::INCREASING) {
                du[i] = 1._f;
            } else {
                du[i] = -1._f;
            }
            // check that energy never goes out of allowed range
            REQUIRE(range.contains(u[i]));
        }
    }

    virtual void create(Storage&, IMaterial&) const override {
        NOT_IMPLEMENTED;
    }
};

template <typename TTimestepping>
static void testClamping(RunSettings settings) {
    SharedPtr<Storage> storage = makeShared<Storage>(getMaterial(MaterialEnum::BASALT));
    storage->insert<Vector>(
        QuantityId::POSITION, OrderEnum::SECOND, Array<Vector>{ Vector(1._f, 0._f, 0._f) });
    storage->insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, 5._f);
    const Interval range(3._f, 7._f);
    IMaterial& material = storage->getMaterial(0);
    material.setRange(QuantityId::ENERGY, range, 0._f);

    settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1._f);
    settings.set(RunSettingsId::TIMESTEPPING_CRITERION, EMPTY_FLAGS);
    TTimestepping timestepping(storage, settings);
    Statistics stats;
    ClampSolver solver1(ClampSolver::Direction::INCREASING, range);
    ThreadPool& pool = *ThreadPool::getGlobalInstance();

    for (Size i = 0; i < 6; ++i) {
        timestepping.step(pool, solver1, stats);
    }
    ArrayView<const Float> u = storage->getValue<Float>(QuantityId::ENERGY);
    REQUIRE(u[0] == range.upper());

    ClampSolver solver2(ClampSolver::Direction::DECREASING, range);
    for (Size i = 0; i < 6; ++i) {
        timestepping.step(pool, solver2, stats);
    }
    REQUIRE(u[0] == range.lower());
}

class AddingParticlesSolver : public ISolver {
public:
    virtual void integrate(Storage& storage, Statistics& UNUSED(stats)) override {
        storage.resize(storage.getParticleCnt() + 100, Storage::ResizeFlag::KEEP_EMPTY_UNCHANGED);

        /// \todo test adding and removing particles from the middle after layered storage is implemented
    }

    virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {}
};


struct TestContext {
    Size callsPerStep = 1;
};

template <typename TTimestepping>
static void testAddingParticles(const RunSettings& settings, TestContext context) {
    // test that solver can add particles during the run without assert/crash
    SharedPtr<Storage> storage = makeShared<Storage>(Tests::getGassStorage(1000));
    const Size particleCnt = storage->getParticleCnt();

    TTimestepping timestepping(storage, settings);
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    AddingParticlesSolver solver;
    Statistics stats;
    for (Size i = 0; i < 5; ++i) {
        REQUIRE_NOTHROW(timestepping.step(pool, solver, stats));
    }
    REQUIRE(storage->getParticleCnt() == particleCnt + context.callsPerStep * 500);
}

template <typename TTimestepping>
static void testAll(RunSettings settings, TestContext context = {}) {
    settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, timeStep);
    settings.set(RunSettingsId::TIMESTEPPING_CRITERION, EMPTY_FLAGS);
    testHomogeneousField<TTimestepping>(settings);
    testHarmonicOscillator<TTimestepping>(settings);
    testGyroscopicMotion<TTimestepping>(settings);
    testClamping<TTimestepping>(settings);
    testAddingParticles<TTimestepping>(settings, context);
}

TEST_CASE("EulerExplicit", "[timestepping]") {
    RunSettings settings;
    testAll<EulerExplicit>(settings);
}

TEST_CASE("PredictorCorrector", "[timestepping]") {
    RunSettings settings;
    testAll<PredictorCorrector>(settings);
}

TEST_CASE("LeapFrog", "[timestepping]") {
    RunSettings settings;
    testAll<LeapFrog>(settings);
}

TEST_CASE("ModifiedMidpoint", "[timestepping]") {
    RunSettings settings;
    TestContext context;

    for (Size n : { 2, 5, 10 }) {
        settings.set(RunSettingsId::TIMESTEPPING_MIDPOINT_COUNT, int(n));
        context.callsPerStep = n;
        testAll<ModifiedMidpointMethod>(settings, context);
    }
}

/// \todo test timestepping of other quantities (first order and sanity check that zero-order quantities
/// remain unchanged).


/*TEST_CASE("RungeKutta", "[timestepping]") {
    Settings<RunSettingsId> settings(GLOBAL_SETTINGS);
    settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, timeStep);
    settings.set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::NONE);
    testHarmonicOscillator<RungeKutta>(settings);
    testGyroscopicMotion<RungeKutta>(settings);
}*/
