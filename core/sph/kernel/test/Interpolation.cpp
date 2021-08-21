#include "sph/kernel/Interpolation.h"
#include "catch.hpp"
#include "objects/geometry/Domain.h"
#include "quantities/Quantity.h"
#include "sph/initial/Distribution.h"
#include "tests/Approx.h"
#include "tests/Setup.h"
#include "utils/SequenceTest.h"

using namespace Sph;

TEST_CASE("Interpolation gassball", "[interpolation]") {
    const Float rho0 = 25._f;
    const Float u0 = 60._f;
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, rho0).set(BodySettingsId::ENERGY, u0);
    Storage storage = Tests::getGassStorage(4000, settings, 1._f);
    SphInterpolant<Float> energyFunc(storage, QuantityId::ENERGY, OrderEnum::ZERO);
    SphInterpolant<Float> densityFunc(storage, QuantityId::DENSITY, OrderEnum::ZERO);

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    const Float h = r[0][H];

    auto test = [&](const Size i) -> Outcome {
        if (getLength(r[i]) > 1._f - 3._f * h) { // kernel radius is just 2, but use 3h to be safe
            return SUCCESS;
        }
        const Float rho = densityFunc.interpolate(r[i]);
        if (rho != approx(rho0, 0.01_f)) {
            return makeFailed("Incorrect density:\n{} == {}", rho, rho0);
        }
        const Float u = energyFunc.interpolate(r[i]);
        if (u != approx(u0, 0.01_f)) {
            return makeFailed("Incorrect energy:\n{} == {}", u, u0);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());

    const Float u_out = energyFunc.interpolate(Vector(2._f, 1._f, 0._f));
    REQUIRE(u_out == 0._f);
}

TEST_CASE("Interpolate velocity", "[interpolation]") {
    const Float rho0 = 30._f;
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, rho0);
    Storage storage = Tests::getGassStorage(4000, settings, 1._f);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    auto field = [](const Vector& x) {
        // some nontrivial velocity field
        return Vector(3._f * x[X] + x[Z], exp(x[Y]) * x[Z], -x[X] / (4._f + x[Z]));
    };
    for (Size i = 0; i < r.size(); ++i) {
        v[i] = field(r[i]);
    }

    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    SphInterpolant<Vector> interpol(storage, QuantityId::POSITION, OrderEnum::FIRST);
    RandomDistribution dist(1234);
    Array<Vector> points = dist.generate(pool, 1000, SphericalDomain(Vector(0._f), 0.7_f));
    auto test = [&](const Size i) -> Outcome {
        const Vector expected = field(points[i]);
        const Vector actual = interpol.interpolate(points[i]);
        if (expected != approx(actual, 0.01_f)) {
            return makeFailed("Incorrect velocity:\n{} == {}", expected, actual);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, points.size());

    const Vector v_out = interpol.interpolate(Vector(-1._f, 2._f, 1._f));
    REQUIRE(v_out == Vector(0._f));
}

TEST_CASE("Corrected interpolation", "[interpolation]") {
    BodySettings settings;
    const Float r0 = 5._f;
    const Float rho0 = 45._f;
    settings.set(BodySettingsId::DENSITY, rho0)
        .set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::RANDOM); // bad uncorrected interpolation
    SphericalDomain domain(Vector(0._f), r0);
    Storage storage = Tests::getGassStorage(10000, settings, domain);
    const Float h0 = storage.getValue<Vector>(QuantityId::POSITION)[0][H];

    SphInterpolant<Float> sphRho(storage, QuantityId::DENSITY, OrderEnum::ZERO);
    CorrectedSphInterpolant<Float> csphRho(storage, QuantityId::DENSITY, OrderEnum::ZERO);

    RandomDistribution dist(1339);
    Array<Vector> points = dist.generate(SEQUENTIAL, 1000, BlockDomain(Vector(0._f), Vector(2._f * r0)));
    auto test = [&](const Size i) -> Outcome {
        const Float corrected = csphRho.interpolate(points[i]);
        const Float uncorrected = sphRho.interpolate(points[i]);

        if (getLength(points[i]) < r0) {
            if (corrected != approx(rho0)) {
                return makeFailed("Incorrect corrected density:\n{} == {}", corrected, rho0);
            }
            if (uncorrected == approx(rho0, 0.0001_f)) {
                return makeFailed("Suspiciously precise uncorrected density??:\n{} == {}", uncorrected, rho0);
            }
        } else if (getLength(points[i]) > r0 + 2 * h0) {
            if (corrected != 0._f || uncorrected != 0._f) {
                return makeFailed("Density outside domain not zero\n{} == {}\n{} == {}",
                    corrected,
                    rho0,
                    uncorrected,
                    rho0);
            }
        } else {
            // in the ring [r0, r0+2h0], there might not be any neighbors (even though it technically lies in
            // the kernel support), so we cannot determine the expected result; just skip
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, points.size());
}
