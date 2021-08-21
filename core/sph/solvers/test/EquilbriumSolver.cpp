#include "catch.hpp"
#include "gravity/SphericalGravity.h"
#include "physics/Constants.h"
#include "physics/Eos.h"
#include "sph/equations/Potentials.h"
#include "sph/solvers/EquilibriumSolver.h"
#include "tests/Approx.h"
#include "tests/Setup.h"
#include "utils/SequenceTest.h"

using namespace Sph;

#ifdef SPH_USE_EIGEN

TEST_CASE("EquilibriumStressSolver no forces", "[equilibriumsolver]") {
    // tests that with no external forces, the stress tensor is zero
    RunSettings settings;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    EquilibriumStressSolver solver(pool, settings, EquationHolder());
    BodySettings body;
    body.set(BodySettingsId::ENERGY, 0._f)
        .set(BodySettingsId::ENERGY_RANGE, Interval(0._f, INFTY))
        .set(BodySettingsId::DENSITY, 10._f);
    Storage storage = Tests::getSolidStorage(1000, body, 1._f * Constants::au);
    solver.create(storage, storage.getMaterial(0));

    Statistics stats;
    REQUIRE(solver.solve(storage, stats));
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
    ArrayView<const TracelessTensor> s = storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ArrayView<const Float> u = storage.getValue<Float>(QuantityId::ENERGY);

    auto test = [&](const Size i) -> Outcome {
        if (p[i] != 0._f || s[i] != TracelessTensor(0._f) || u[i] != 0._f) {
            return makeFailed(
                "Invalid solutions for r = ", r[i], "\n p = ", p[i], "\n u = ", u[i], "\n s = ", s[i]);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}

TEST_CASE("EquilibriumStressSolver pressure", "[equilibriumsolver]") {
    // tests that in a sphere with gravity and pressure gradient, the pressure distribution follows the
    // analytical result (considering EoS rho = const.)

    RunSettings settings;
    const Float rho0 = 300._f;
    const Float r0 = 1._f * Constants::au;
    EquationHolder equations = makeTerm<SphericalGravityEquation>();
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    EquilibriumStressSolver solver(pool, settings, std::move(equations));

    BodySettings body;
    // body.set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::DIEHL_ET_AL);
    // zero shear modulus to get only pressure without other components of the stress tensor
    body.set(BodySettingsId::SHEAR_MODULUS, 0._f).set(BodySettingsId::DENSITY, rho0);
    Storage storage = Tests::getGassStorage(1000, body, r0);
    solver.create(storage, storage.getMaterial(0));

    Statistics stats;
    REQUIRE(solver.solve(storage, stats));

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Float> p = storage.getValue<Float>(QuantityId::PRESSURE);

    Float K = 0._f;
    Analytic::StaticSphere sphere(r0, rho0);
    auto expected = [&](const Float x) { //
        return K + sphere.getPressure(x);
    };
    // find offset
    /// \todo this is extra step, we should specify boundary conditions
    Float offset = 0._f;
    Size cnt = 0;
    for (Size i = 0; i < r.size(); ++i) {
        if (getLength(r[i]) < 0.7_f * r0) {
            offset += p[i] - expected(getLength(r[i]));
            cnt++;
        }
    }
    K = offset / cnt;

    auto test = [&](const Size i) -> Outcome { //
        if (getLength(r[i]) > 0.7_f * Constants::au) {
            return SUCCESS;
        }
        const Float p0 = expected(getLength(r[i]));
        if (p[i] != approx(p0, 0.05_f)) {
            return makeFailed("Incorrect pressure: \n{} == {}", p[i], p0);
        }
        return SUCCESS;
    };

    FileLogger logger(Path("p.txt"));
    ArrayView<Size> neighCnts = storage.getValue<Size>(QuantityId::NEIGHBOR_CNT);
    for (Size i = 0; i < r.size(); ++i) {
        logger.write(getLength(r[i]), "  ", p[i], "  ", neighCnts[i]);
    }

    REQUIRE_SEQUENCE(test, 0, r.size());
}

TEST_CASE("EquilibriumStressSolver stationary", "[equilibriumsolver]") {
    // tests that the solution of static solver is indeed the statinary solution, meaning the derivatives of
    // density, energy and stress tensor will be (approximately) zero in the first time step

    const Float rho0 = 2700._f;
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, rho0);
    Storage storage = Tests::getSolidStorage(1000, settings, 1.e5_f);

    /*TillotsonEos eos(BodySettings::getDefaults());
    const Float u0 = BodySettings::getDefaults().get<Float>(BodySettingsId::TILLOTSON_SUBLIMATION);
    FileLogger logger("eos.txt");
    for (Float u = 0; u < u0; u += 1.e-3_f * u0) {
        logger.write(u, "  ", eos.evaluate(2700, u)[0]);
    }*/

    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    EquationHolder equations;
    equations += makeTerm<SphericalGravityEquation>();
    equations += makeTerm<InertialForce>(Vector(0._f, 0._f, 2.f * PI / (3600._f * 12._f)));
    EquilibriumStressSolver staticSolver(pool, RunSettings::getDefaults(), equations);
    staticSolver.create(storage, storage.getMaterial(0));
    Statistics stats;
    /*REQUIRE_NOTHROW(staticSolver.solve(storage, stats));

    RunSettings settings;
    equations += makeTerm<PressureForce>() + makeTerm<SolidStressForce>(settings) +
                 makeTerm<ContinuityEquation>(settings);
    GenericSolver genericSolver(settings, equations);*/
}

#endif
