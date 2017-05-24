#include "sph/equations/av/Stress.h"
#include "catch.hpp"
#include "timestepping/TimeStepping.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"
#include "utils/Setup.h"

using namespace Sph;

TEST_CASE("StressAV test", "[av]") {
    // prepare storage, two hemispheres moving towards each other in x direction
    const Float u0 = 0._f;
    BodySettings body;
    body.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE);
    /// \todo this test is HIGHLY sensitive on initial distribution! It fails if we select a different option
    /// (symmetry along the axes is essenstial)
    body.set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::CUBIC);
    SharedPtr<Storage> storage = makeShared<Storage>(Tests::getSolidStorage(10000, body, 1._f, 1._f, u0));
    const Float cs = storage->getValue<Float>(QuantityId::SOUND_SPEED)[0];
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITIONS);
    for (Size i = 0; i < r.size(); ++i) {
        // subsonic flow away from x=0
        v[i] = Vector(r[i][X] < 0._f ? -0.1_f * cs : 0.1_f * cs, 0._f, 0._f);
    }
    RunSettings settings;
    settings.set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::NONE);
    settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.1_f * r[0][H] / cs);
    EulerExplicit timestepping(storage, settings);

    // solver with some basic forces and artificial stress
    EquationHolder eqs;
    eqs += makeTerm<PressureForce>() + makeTerm<SolidStressForce>(settings) +
           makeTerm<ContinuityEquation<DensityEvolution::SOLID>>() + makeTerm<StressAV>(settings);
    GenericSolver solver(settings, std::move(eqs));
    solver.create(*storage, storage->getMaterial(0));

    // do one time step to compute values of stress tensor
    Statistics stats;
    timestepping.step(solver, stats);

    // sanity check - check components of stress tensor
    // note that artificial stress shouldn't do anything so far as we have zero initial stress tensor
    ArrayView<TracelessTensor> s = storage->getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITIONS);
    ArrayView<SymmetricTensor> as = storage->getValue<SymmetricTensor>(QuantityId::AV_STRESS);
    const Float h = r[0][H];
    auto test1 = [&](const Size i) -> Outcome {
        if (abs(r[i][X]) < h && s[i] == TracelessTensor::null()) {
            return makeFailed("Zero components of stress tensor in shock front");
        }
        if (abs(r[i][X]) > 3._f * h && s[i] != TracelessTensor::null()) {
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
    eqs = makeTerm<StressAV>(settings);
    GenericSolver solverAS(settings, std::move(eqs));
    timestepping.step(solverAS, stats);

    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITIONS);
    as = storage->getValue<SymmetricTensor>(QuantityId::AV_STRESS);
    auto test2 = [&](const Size i) -> Outcome {
        if (getLength(r[i]) > 0.7_f) {
            // skip boundary layer
            return SUCCESS;
        }
        if (abs(r[i][X]) < h) {
            const Float ratio = (abs(dv[i][Y]) + abs(dv[i][Z])) / abs(dv[i][X]);
            if (ratio > 1.e-3_f) {
                return makeFailed("Acceleration does not have x direction: ", dv[i], "\n ratio = ", ratio);
            }
            // acceleration should be in the opposite direction than the velocity
            if (r[i][X] <= 0._f) {
                if (as[i] == SymmetricTensor::null() || dv[i][X] > -1.e6_f) {
                    return makeFailed("Incorrect acceleration in x>0: ", dv[i]);
                }
            } else if (r[i][X] > 0._f) {
                if (as[i] == SymmetricTensor::null() || dv[i][X] < 1.e6_f) {
                    return makeFailed("Incorrect acceleration in x<0: ", dv[i], "\nr=", r[i], "\nAS=", as[i]);
                }
            }
        } else if (abs(r[i][X]) > 2._f * h && dv[i] != approx(Vector(0._f))) {
            return makeFailed("Accelerated where it shouldn't", dv[i]);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test2, 0, r.size());
}
