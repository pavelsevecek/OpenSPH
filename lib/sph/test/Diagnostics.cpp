#include "sph/Diagnostics.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "quantities/Storage.h"
#include "sph/initial/Initial.h"
#include "system/Settings.h"

using namespace Sph;

TEST_CASE("Pairing", "[diagnostics]") {
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, GlobalSettings::getDefaults());
    BodySettings settings;
    settings.set(BodySettingsIds::PARTICLE_COUNT, 100);
    conds.addBody(SphericalDomain(Vector(0._f), 3._f), settings);
    Array<Vector>& r = storage->getValue<Vector>(QuantityIds::POSITIONS);

    ParticlePairing diag(2, 1.e-1_f);
    REQUIRE(diag.getPairs(*storage).empty());

    const Size n = r.size();
    r.push(r[55]); // -> pair n, 55
    r.push(r[68]); // -> pair n+1, 68
    r.push(r[12]); // -> pair n+2, 12

    diag = ParticlePairing(2, 1.e-2_f);
    Array<ParticlePairing::Pair> pairs = diag.getPairs(*storage);
    REQUIRE(pairs.size() == 3);
    std::sort(pairs.begin(), pairs.end(), [](auto&& p1, auto&& p2) {
        // sort by lower indices
        return min(p1.i1, p1.i2) < min(p2.i1, p2.i2);
    });
    REQUIRE(min(pairs[0].i1, pairs[0].i2) == 12);
    REQUIRE(max(pairs[0].i1, pairs[0].i2) == n + 2);
    REQUIRE(min(pairs[1].i1, pairs[1].i2) == 55);
    REQUIRE(max(pairs[1].i1, pairs[1].i2) == n);
    REQUIRE(min(pairs[2].i1, pairs[2].i2) == 68);
    REQUIRE(max(pairs[2].i1, pairs[2].i2) == n + 1);
}