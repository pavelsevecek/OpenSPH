#include "sph/solvers/GravitySolver.h"
#include "catch.hpp"
#include "gravity/BarnesHut.h"
#include "gravity/BruteForceGravity.h"
#include "gravity/Moments.h"
#include "gravity/SphericalGravity.h"
#include "objects/Exceptions.h"
#include "sph/boundary/Boundary.h"
#include "sph/solvers/SymmetricSolver.h"
#include "tests/Approx.h"
#include "tests/Setup.h"
#include "utils/SequenceTest.h"

using namespace Sph;

static void testGravity(AutoPtr<IGravity>&& gravity) {
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, 1._f).set(BodySettingsId::ENERGY, 1._f);
    Storage storage = Tests::getGassStorage(2000, settings, Constants::au);
    ThreadPool& pool = *ThreadPool::getGlobalInstance();

    // no SPH equations, just gravity
    GravitySolver<SymmetricSolver<3>> solver(pool,
        RunSettings::getDefaults(),
        makeTerm<ConstSmoothingLength>(),
        makeAuto<NullBoundaryCondition>(),
        std::move(gravity));
    REQUIRE_NOTHROW(solver.create(storage, storage.getMaterial(0)));
    Statistics stats;
    REQUIRE_NOTHROW(solver.integrate(storage, stats));

    // only gravity, no pressure -> gass cloud should collapse, acceleration to the center
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);

    auto test = [&](const Size i) -> Outcome {
        if (getLength(dv[i]) == 0._f) {
            return makeFailed("No acceleration for particle {}", i);
        }
        if (getLength(r[i]) > EPS) {
            // check acceleration direction: dv ~ -r
            // avoid numerical issues for particles close to the center
            const Vector r0 = getNormalized(r[i]);
            const Vector dv0 = getNormalized(dv[i]);
            /// \todo this is quite imprecise, is it to be expected?
            if (dv0 != approx(-r0, 0.1_f)) {
                return makeFailed(
                    "Incorrect acceleration direction for particle {}\n r0 == {}\n dv == {}", i, r0, dv0);
            }
        }
        // check magnitude of acceleration
        const Float rho0 = storage.getMaterial(0)->getParam<Float>(BodySettingsId::DENSITY);
        const Float M = sphereVolume(getLength(r[i])) * rho0;
        const Float expected = Constants::gravity * M / getSqrLength(r[i]);
        const Float actual = getLength(dv[i]);
        /// \todo actual value seems to be under-estimated, is there some sort of discretization bias?
        if (actual != approx(expected, 0.1_f)) {
            return makeFailed(
                "Incorrect acceleration magnitude for particle {}\n{} == {}", i, actual, expected);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}

TEST_CASE("GravitySolver", "[solvers]") {
    testGravity(makeAuto<BruteForceGravity>());
    testGravity(makeAuto<BruteForceGravity>(GravityKernel<CubicSpline<3>>{}));
    testGravity(makeAuto<BarnesHut>(0.5_f, MultipoleOrder::QUADRUPOLE));
    testGravity(makeAuto<BarnesHut>(0.5_f, MultipoleOrder::QUADRUPOLE, GravityKernel<CubicSpline<3>>{}));
}

TEST_CASE("GravitySolver setup", "[solvers]") {
    EquationHolder holder;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    holder += makeTerm<SphericalGravityEquation>();
    RunSettings settings;
    Storage storage = Tests::getGassStorage(2);
    GravitySolver<SymmetricSolver<3>> solver(
        pool, settings, holder, makeAuto<NullBoundaryCondition>(), makeAuto<BruteForceGravity>());
    REQUIRE_THROWS_AS(solver.create(storage, storage.getMaterial(0)), InvalidSetup);
}
