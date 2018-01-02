#include "catch.hpp"
#include "objects/geometry/Domain.h"
#include "objects/utility/PerElementWrapper.h"
#include "sph/initial/Initial.h"
#include "sph/solvers/ContinuitySolver.h"
#include "tests/Setup.h"
#include "timestepping/TimeStepping.h"

using namespace Sph;


TEST_CASE("Impact", "[impact]]") {
    // Check that first two steps of impact work as expected.

    RunSettings settings;
    settings.set(RunSettingsId::MODEL_FORCE_SOLID_STRESS, true);
    EquationHolder eqs;
    /// \todo refactor, avoid adding ConstSmootihngLength term
    eqs += makeTerm<PressureForce>() + makeTerm<SolidStressForce>(settings) + makeTerm<StandardAV>() +
           makeTerm<ContinuityEquation>(settings) + makeTerm<ConstSmoothingLength>();
    SymmetricSolver solver(settings, std::move(eqs));

    SharedPtr<Storage> storage = makeShared<Storage>();
    InitialConditions initial(solver, settings);
    BodySettings body;
    body.set(BodySettingsId::PARTICLE_COUNT, 1000);
    body.set(BodySettingsId::ENERGY, 0._f);
    body.set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::SCALAR_GRADY_KIPP);
    body.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES);
    const Float rho0 = body.get<Float>(BodySettingsId::DENSITY);

    initial.addMonolithicBody(*storage, SphericalDomain(Vector(0._f), 1._f), body);
    body.set(BodySettingsId::PARTICLE_COUNT, 10);
    // bodies overlap a bit, that's OK
    initial.addMonolithicBody(*storage, SphericalDomain(Vector(1._f, 0._f, 0._f), 0.1_f), body)
        .addVelocity(Vector(-5._f, 0._f, 0._f));

    EulerExplicit timestepping(storage, settings);
    Statistics stats;

    // 1. After first step, the StrengthVelocityGradient should be zero, meaning derivatives of stress tensor
    //    and density should be zero as well (and therefore values as well)

    timestepping.step(solver, stats);

    ArrayView<SymmetricTensor> gradv =
        storage->getValue<SymmetricTensor>(QuantityId::STRENGTH_VELOCITY_GRADIENT);
    ArrayView<TracelessTensor> s, ds;
    tie(s, ds) = storage->getAll<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ArrayView<Float> rho, drho;
    tie(rho, drho) = storage->getAll<Float>(QuantityId::DENSITY);

    REQUIRE(perElement(gradv) == SymmetricTensor::null());
    REQUIRE(perElement(s) == TracelessTensor::null());
    REQUIRE(perElement(ds) == TracelessTensor::null());
    REQUIRE(perElement(rho) == rho0);
    REQUIRE(perElement(drho) == 0._f);

    // 2. Derivatives are nonzero in the second step (as there is already a nonzero velocity gradient inside
    //    each body)

    timestepping.step(solver, stats);

    // arrayviews must be reset as they might be (and were) invalidated
    gradv = storage->getValue<SymmetricTensor>(QuantityId::STRENGTH_VELOCITY_GRADIENT);
    tie(s, ds) = storage->getAll<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    tie(rho, drho) = storage->getAll<Float>(QuantityId::DENSITY);
    // not all particles will be affected by the impact yet; count number of nonzero particles
    /// \todo check only particles close to impact

    Size gradvCnt = 0;
    Size sCnt = 0, dsCnt = 0;
    Size rhoCnt = 0, drhoCnt = 0;
    for (Size i = 0; i < rho.size(); ++i) {
        gradvCnt += int(gradv[i] != SymmetricTensor::null());
        sCnt += int(s[i] != TracelessTensor::null());
        dsCnt += int(ds[i] != TracelessTensor::null());
        rhoCnt += int(rho[i] != rho0);
        drhoCnt += int(drho[i] != 0._f);
    }

    REQUIRE(gradvCnt > 50);
    REQUIRE(sCnt > 50);
    REQUIRE(dsCnt > 50);
    REQUIRE(rhoCnt > 50);
    REQUIRE(drhoCnt > 50);
}
