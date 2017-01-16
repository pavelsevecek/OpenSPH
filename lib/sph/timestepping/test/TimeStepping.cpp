#include "sph/timestepping/TimeStepping.h"
#include "catch.hpp"
#include "quantities/Storage.h"
#include "solvers/AbstractSolver.h"
#include "system/Logger.h"

using namespace Sph;


struct HomogeneousField : public Abstract::Solver {
    Vector g = Vector(0.f, 0.f, 1._f);

    HomogeneousField() = default;

    virtual void integrate(Storage& storage) override {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
        for (Size i = 0; i < r.size(); ++i) {
            dv[i] = g;
        }
    }

    virtual void initialize(Storage&, const BodySettings&) const override { NOT_IMPLEMENTED; }
};

struct HarmonicOscillator : public Abstract::Solver {
    Float period = 1._f;

    HarmonicOscillator() = default;

    virtual void integrate(Storage& storage) override {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
        Float omega = 2._f * PI / period;
        for (Size i = 0; i < r.size(); ++i) {
            dv[i] = -sqr(omega) * r[i];
        }
    }

    virtual void initialize(Storage&, const BodySettings&) const override { NOT_IMPLEMENTED; }
};

struct LorentzForce : public Abstract::Solver {
    const Vector B = Vector(0.f, 0.f, 1.f);

    LorentzForce() = default;

    virtual void integrate(Storage& storage) override {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
        for (Size i = 0; i < r.size(); ++i) {
            dv[i] = cross(v[i], B);
        }
    }

    virtual void initialize(Storage&, const BodySettings&) const override { NOT_IMPLEMENTED; }
};

const Float timeStep = 0.01_f;


template <typename TTimestepping, typename... TArgs>
void testHomogeneousField(TArgs&&... args) {
    HomogeneousField solver;

    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    storage->emplace<Vector, OrderEnum::SECOND_ORDER>(
        QuantityIds::POSITIONS, Array<Vector>{ Vector(0._f, 0._f, 0._f) });

    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityIds::POSITIONS);
    TTimestepping timestepping(storage, std::forward<TArgs>(args)...);
    FrequentStats stats;
    Size n = 0;
    StdOutLogger logger;
    bool allMatching = true;
    for (float t = 0.f; t < 3.f; t += timestepping.getTimeStep()) {
        const Vector pos(0.f, 0.f, 0.5 * sqr(t));
        const Vector vel(0.f, 0.f, t);
        if (!almostEqual(r[0], pos, 2.f * timeStep)) {
            logger.write("Homogeneous field:");
            logger.write("Invalid position: ", r[0], " / ", pos);
            logger.write("t = ", t);
            allMatching = false;
            break;
        }
        if (!almostEqual(v[0], vel, timeStep)) {
            logger.write("Homogeneous field:");
            logger.write("Invalid velocity: ", v[0], " / ", vel);
            logger.write("t = ", t);
            allMatching = false;
            break;
        }
        timestepping.step(solver, stats);
    }
    REQUIRE(allMatching);
}

template <typename TTimestepping, typename... TArgs>
void testHarmonicOscillator(TArgs&&... args) {
    HarmonicOscillator solver;

    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    storage->emplace<Vector, OrderEnum::SECOND_ORDER>(
        QuantityIds::POSITIONS, Array<Vector>{ Vector(1._f, 0._f, 0._f) });

    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityIds::POSITIONS);
    TTimestepping timestepping(storage, std::forward<TArgs>(args)...);
    FrequentStats stats;
    Size n = 0;
    StdOutLogger logger;
    bool allMatching = true;
    for (float t = 0.f; t < 3.f; t += timestepping.getTimeStep()) {
        if (!almostEqual(r[0], Vector(cos(2.f * PI * t), 0.f, 0.f), timeStep * 2._f * PI)) {
            logger.write("Harmonic oscillator:");
            logger.write("Invalid position: ", r[0], " / ", Vector(cos(2.f * PI * t), 0.f, 0.f));
            logger.write("t = ", t);
            allMatching = false;
            break;
        }
        if (!almostEqual(v[0], Vector(-sin(2.f * PI * t) * 2.f * PI, 0.f, 0.f), timeStep * sqr(2._f * PI))) {
            logger.write("Harmonic oscillator:");
            logger.write("Invalid velocity: ", v[0], " / ", Vector(-sin(2.f * PI * t), 0.f, 0.f));
            logger.write("t = ", t);
            allMatching = false;
            break;
        }
        timestepping.step(solver, stats);
    }
    REQUIRE(allMatching);
}

template <typename TTimestepping, typename... TArgs>
void testGyroscopicMotion(TArgs&&... args) {
    LorentzForce solver;

    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    storage->emplace<Vector, OrderEnum::SECOND_ORDER>(
        QuantityIds::POSITIONS, Array<Vector>{ Vector(1._f, 0._f, 0._f) });

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityIds::POSITIONS);
    v[0] = Vector(0._f, -1._f, 0.5_f);
    // y component is perpendicular, should oscilate with cyclotron frequency, in this case omega_C = B
    // z component is parallel, should move with constant velocity

    TTimestepping timestepping(storage, std::forward<TArgs>(args)...);
    FrequentStats stats;
    Size n = 0;
    StdOutLogger logger;
    bool allMatching = true;
    for (float t = 0.f; t < 3.f; t += timestepping.getTimeStep()) {
        const Vector pos = Vector(cos(t), -sin(t), 0.5_f * t);
        const Vector vel = Vector(-sin(t), -cos(t), 0.5_f);
        if (!almostEqual(r[0], pos, 3.f * timeStep)) {
            logger.write("Gyroscopic motion:");
            logger.write("Invalid position: ", r[0], " / ", pos);
            logger.write("t = ", t);
            allMatching = false;
            break;
        }
        if (!almostEqual(v[0], vel, 3.f * timeStep)) {
            logger.write("Gyroscopic motion:");
            logger.write("Invalid velocity: ", v[0], " / ", vel);
            logger.write("t = ", t);
            allMatching = false;
            break;
        }
        timestepping.step(solver, stats);
    }
    REQUIRE(allMatching);
}

TEST_CASE("EulerExplicit", "[timestepping]") {
    Settings<GlobalSettingsIds> settings(GLOBAL_SETTINGS);
    settings.set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, timeStep);
    testHomogeneousField<EulerExplicit>(settings);
    testHarmonicOscillator<EulerExplicit>(settings);
    testGyroscopicMotion<EulerExplicit>(settings);
}

TEST_CASE("PredictorCorrector", "[timestepping]") {
    Settings<GlobalSettingsIds> settings(GLOBAL_SETTINGS);
    settings.set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, timeStep);
    testHomogeneousField<PredictorCorrector>(settings);
    testHarmonicOscillator<PredictorCorrector>(settings);
    testGyroscopicMotion<PredictorCorrector>(settings);
}

/// \todo test timestepping of other quantities (first order and sanity check that zero-order quantities
/// remain unchanged).


/*TEST_CASE("RungeKutta", "[timestepping]") {
    Settings<GlobalSettingsIds> settings(GLOBAL_SETTINGS);
    settings.set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, timeStep);
    testHarmonicOscillator<RungeKutta>(settings);
    testGyroscopicMotion<RungeKutta>(settings);
}*/
