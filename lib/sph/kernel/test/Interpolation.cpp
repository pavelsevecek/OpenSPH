#include "sph/kernel/Interpolation.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "sph/initial/Distribution.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"
#include "utils/Setup.h"

using namespace Sph;

TEST_CASE("Interpolation gassball", "[interpolation]") {
    const Float rho0 = 25._f;
    const Float u0 = 60._f;
    Storage storage = Tests::getGassStorage(4000, BodySettings::getDefaults(), 1._f, rho0, u0);
    Interpolation interpol(storage);

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    const Float h = r[0][H];

    auto test = [&](const Size i) -> Outcome {
        if (getLength(r[i]) > 1._f - 3._f * h) { // kernel radius is just 2, but use 3h to be safe
            return SUCCESS;
        }
        const Float rhoInt = interpol.interpolate<Float>(QuantityId::DENSITY, OrderEnum::ZERO, r[i]);
        if (rhoInt != approx(rho0, 0.01_f)) {
            return makeFailed("Incorrect density:\n", rhoInt, " == ", rho0);
        }
        const Float uInt = interpol.interpolate<Float>(QuantityId::ENERGY, OrderEnum::ZERO, r[i]);
        if (uInt != approx(u0, 0.01_f)) {
            return makeFailed("Incorrect energy:\n", uInt, " == ", u0);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}

TEST_CASE("Interpolate velocity", "[interpolation]") {
    const Float rho0 = 30._f;
    Storage storage = Tests::getGassStorage(4000, BodySettings::getDefaults(), 1._f, rho0);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
    auto field = [](const Vector& x) {
        // some nontrivial velocity field
        return Vector(3._f * x[X] + x[Z], exp(x[Y]) * x[Z], -x[X] / (4._f + x[Z]));
    };
    for (Size i = 0; i < r.size(); ++i) {
        v[i] = field(r[i]);
    }

    Interpolation interpol(storage);

    RandomDistribution dist;
    Array<Vector> points = dist.generate(1000, SphericalDomain(Vector(0._f), 0.7_f));
    auto test = [&](const Size i) -> Outcome {
        const Vector expected = field(points[i]);
        const Vector actual =
            interpol.interpolate<Vector>(QuantityId::POSITIONS, OrderEnum::FIRST, points[i]);
        if (expected != approx(actual, 0.01_f)) {
            return makeFailed("Incorrect velocity:\n", expected, " == ", actual);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, points.size());
}
