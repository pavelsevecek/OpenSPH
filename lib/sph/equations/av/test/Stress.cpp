#include "sph/equations/av/Stress.h"
#include "catch.hpp"
#include "sph/solvers/SymmetricSolver.h"
#include "system/Statistics.h"
#include "tests/Approx.h"
#include "tests/Setup.h"
#include "timestepping/TimeStepping.h"
#include "utils/SequenceTest.h"

using namespace Sph;

TEST_CASE("StressAV test", "[av]") {
    // prepare storage, two hemispheres moving towards each other in x direction
    const Float u0 = 0._f;
    BodySettings body;
    body.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE);
    /// \todo this test is HIGHLY sensitive on initial distribution!
    body.set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::HEXAGONAL)
        .set(BodySettingsId::DENSITY, 1._f)
        .set(BodySettingsId::ENERGY, 0._f);
    SharedPtr<Storage> storage = makeShared<Storage>(Tests::getSolidStorage(10000, body, 1._f));
    const Float cs = storage->getValue<Float>(QuantityId::SOUND_SPEED)[0];
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
    const Vector dir = getNormalized(Vector(1._f, 2._f, -5._f)); // some non-trivial direction of motion
    for (Size i = 0; i < r.size(); ++i) {
        // subsonic flow along the axis
        v[i] = dir * (dot(r[i], dir) > 0._f ? 0.1_f * cs : -0.1_f * cs);
    }
    RunSettings settings;
    settings.set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::NONE);
    settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.1_f * r[0][H] / cs);
    settings.set(RunSettingsId::MODEL_FORCE_SOLID_STRESS, true);
    EulerExplicit timestepping(storage, settings);

    // solver with some basic forces and artificial stress
    EquationHolder eqs;
    eqs += makeTerm<PressureForce>() + makeTerm<SolidStressForce>(settings) +
           makeTerm<ContinuityEquation>(settings) + makeTerm<StressAV>(settings) +
           makeTerm<ConstSmoothingLength>();
    SymmetricSolver solver(settings, std::move(eqs));
    solver.create(*storage, storage->getMaterial(0));

    // do one time step to compute values of stress tensor
    Statistics stats;
    timestepping.step(solver, stats);

    // sanity check - check components of stress tensor
    // note that artificial stress shouldn't do anything so far as we have zero initial stress tensor
    ArrayView<TracelessTensor> s = storage->getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
    ArrayView<SymmetricTensor> as = storage->getValue<SymmetricTensor>(QuantityId::AV_STRESS);
    const Float h = r[0][H];
    auto test1 = [&](const Size i) -> Outcome {
        if (abs(dot(r[i], dir)) < h && s[i] == TracelessTensor::null()) {
            return makeFailed("Zero components of stress tensor in shock front");
        }
        if (abs(dot(r[i], dir)) > 3._f * h && s[i] != TracelessTensor::null()) {
            return makeFailed("Non-zero components of stress tensor far from shock front");
        }
        if (as[i] != SymmetricTensor::null()) {
            return makeFailed("Non-zero artificial stress after first step");
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test1, 0, r.size());

    // do another step - this time we should get nonzero artificial stress
    // create another solver WITHOUT pressure and stress force to get acceleration only from AS
    eqs = makeTerm<StressAV>(settings) + makeTerm<ConstSmoothingLength>();
    SymmetricSolver solverAS(settings, std::move(eqs));
    timestepping.step(solverAS, stats);

    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
    as = storage->getValue<SymmetricTensor>(QuantityId::AV_STRESS);
    auto test2 = [&](const Size i) -> Outcome {
        if (getLength(r[i]) > 0.7_f) {
            // skip boundary layer
            return SUCCESS;
        }
        if (abs(dot(r[i], dir)) < h) {
            const Float ratio = (abs(distance(dv[i], dir))) / abs(dot(dv[i], dir));
            if (ratio > 0.2_f) {
                /// \todo can we achieve better accuracy than this??
                return makeFailed(
                    "Acceleration does not have correct direction: ", dv[i], "\n ratio = ", ratio);
            }
            // acceleration should be in the opposite direction than the velocity
            if (dot(r[i], dir) <= 0._f) {
                if (as[i] == SymmetricTensor::null() || dot(dv[i], dir) > -1.e6_f) {
                    return makeFailed("Incorrect acceleration in dot>0: ", dv[i]);
                }
            } else if (dot(r[i], dir) > 0._f) {
                if (as[i] == SymmetricTensor::null() || dot(dv[i], dir) < 1.e6_f) {
                    return makeFailed(
                        "Incorrect acceleration in dot<0: ", dv[i], "\nr=", r[i], "\nAS=", as[i]);
                }
            }
        } else if (abs(dot(r[i], dir)) > 2._f * h && dv[i] != approx(Vector(0._f), 1._f)) {
            return makeFailed("Accelerated where it shouldn't", dv[i]);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test2, 0, r.size());
}
