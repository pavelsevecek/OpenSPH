#include "sph/solvers/StaticSolver.h"
#include "catch.hpp"
#include "physics/Constants.h"
#include "sph/equations/Potentials.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"
#include "utils/Setup.h"


#include "io/Logger.h"
#include "physics/Eos.h"

using namespace Sph;

TEST_CASE("StaticSolver pressure", "[staticsolver]") {
    // tests that in a sphere with gravity and pressure gradient, the pressure distribution follows the
    // analytical result (considering EoS rho = const.)

    RunSettings settings;
    const Float rho0 = 300._f;
    EquationHolder equations =
        makeTerm<SphericalGravity>(SphericalGravity::Options::ASSUME_HOMOGENEOUS); //(std::move(potential);
    StaticSolver solver(settings, std::move(equations));

    BodySettings body;
    // body.set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::DIEHL_ET_AL);
    // zero shear modulus to get only pressure without other components of the stress tensor
    body.set(BodySettingsId::SHEAR_MODULUS, 0._f);
    Storage storage = Tests::getGassStorage(1000, body, 1._f * Constants::au, rho0);
    solver.create(storage, storage.getMaterial(0));

    Statistics stats;
    REQUIRE(solver.solve(storage, stats));

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    ArrayView<const Float> p = storage.getValue<Float>(QuantityId::PRESSURE);

    Float K = 0._f;
    auto expected = [&](const Float x) { //
        return K - 2._f / 3._f * PI * Constants::gravity * sqr(rho0) * sqr(x);
    };
    // find offset
    /// \todo this is extra step, we should specify boundary conditions
    Float offset = 0._f;
    Size cnt = 0;
    for (Size i = 0; i < r.size(); ++i) {
        if (getLength(r[i]) < 0.7_f * Constants::au) {
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
            return makeFailed("Incorrect pressure: \n", p[i], " == ", p0);
        }
        return SUCCESS;
    };

    FileLogger logger("p.txt");
    ArrayView<Size> neighCnts = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);
    for (Size i = 0; i < r.size(); ++i) {
        logger.write(getLength(r[i]), "  ", p[i], "  ", neighCnts[i]);
    }

    REQUIRE_SEQUENCE(test, 0, r.size());
}

TEST_CASE("StaticSolver stationary", "[staticsolver]") {
    // tests that the solution of static solver is indeed the statinary solution, meaning the derivatives of
    // density, energy and stress tensor will be (approximately) zero in the first time step

    const Float rho0 = 2700._f;
    Storage storage = Tests::getSolidStorage(1000, BodySettings::getDefaults(), 1.e5_f, rho0);

    /*TillotsonEos eos(BodySettings::getDefaults());
    const Float u0 = BodySettings::getDefaults().get<Float>(BodySettingsId::TILLOTSON_SUBLIMATION);
    FileLogger logger("eos.txt");
    for (Float u = 0; u < u0; u += 1.e-3_f * u0) {
        logger.write(u, "  ", eos.evaluate(2700, u)[0]);
    }*/

    EquationHolder equations;
    equations += makeTerm<SphericalGravity>(SphericalGravity::Options::ASSUME_HOMOGENEOUS);
    equations += makeTerm<NoninertialForce>(Vector(0._f, 0._f, 2.f * PI / (3600._f * 12._f)));
    StaticSolver staticSolver(RunSettings::getDefaults(), equations);
    staticSolver.create(storage, storage.getMaterial(0));
    Statistics stats;
    /*REQUIRE_NOTHROW(staticSolver.solve(storage, stats));

    RunSettings settings;
    equations += makeTerm<PressureForce>() + makeTerm<SolidStressForce>(settings) +
                 makeTerm<ContinuityEquation>(settings);
    GenericSolver genericSolver(settings, equations);*/
}
