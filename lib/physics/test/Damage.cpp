#include "physics/Damage.h"
#include "catch.hpp"
#include "math/rng/Rng.h"
#include "objects/geometry/Domain.h"
#include "objects/utility/ArrayUtils.h"
#include "physics/Rheology.h"
#include "quantities/Storage.h"
#include "sph/Material.h"
#include "sph/initial/Distribution.h"
#include "sph/initial/Initial.h"
#include "system/ArrayStats.h"
#include "system/Settings.h"
#include "tests/Approx.h"

using namespace Sph;


TEST_CASE("Distribute flaws", "[damage]") {
    ScalarDamage model(2._f);
    BodySettings bodySettings;
    Storage storage(getDefaultMaterial());
    HexagonalPacking distribution;
    SphericalDomain domain(Vector(0._f), 1._f);
    Array<Vector> r = distribution.generate(9000, domain);
    const int N = r.size();
    storage.insert<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND, std::move(r));
    const Float rho0 = bodySettings.get<Float>(BodySettingsId::DENSITY);
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, rho0);
    storage.insert<Float>(QuantityId::MASSES, OrderEnum::ZERO, rho0 * domain.getVolume() / N);
    MaterialInitialContext context;
    context.rng = makeAuto<RngWrapper<BenzAsphaugRng>>(1234);
    model.setFlaws(storage, storage.getMaterial(0), context);

    // check that all particles have at least one flaw
    ArrayView<Size> n_flaws = storage.getValue<Size>(QuantityId::N_FLAWS);
    REQUIRE(areAllMatching(n_flaws, [N](const int n) { return n >= 1 && n <= N; }));

    // check that the total number of flaws
    /// \todo how does this depend on N?
    int n_total = 0;
    for (int n : n_flaws) {
        n_total += n;
    }
    const Float m_weibull = bodySettings.get<Float>(BodySettingsId::WEIBULL_EXPONENT);
    REQUIRE(n_total >= 8._f * N);
    REQUIRE(n_total <= 11._f * N);

    ArrayStats<Float> mStats(storage.getValue<Float>(QuantityId::M_ZERO));
    ArrayStats<Float> growthStats(storage.getValue<Float>(QuantityId::EXPLICIT_GROWTH));
    ArrayStats<Float> epsStats(storage.getValue<Float>(QuantityId::EPS_MIN));
    REQUIRE(mStats.min() == 1._f);
    REQUIRE(mStats.max() > m_weibull);
    REQUIRE(mStats.average() == approx(m_weibull, 0.05_f));
    REQUIRE(growthStats.min() == growthStats.max());
    REQUIRE(epsStats.min() > 0._f);
    REQUIRE(epsStats.max() == approx(3.e-4_f, 0.2_f));
}

TEST_CASE("Fracture growth", "[damage]") {
    /// \todo some better test, for now just testing that integrate will work without asserts
    ScalarDamage damage(2._f);
    Storage storage;
    InitialConditions conds(storage, RunSettings::getDefaults());
    conds.addBody(SphericalDomain(Vector(0._f), 1._f), BodySettings::getDefaults());

    MaterialInitialContext context;
    context.rng = makeAuto<RngWrapper<BenzAsphaugRng>>(1234);
    MaterialView material = storage.getMaterial(0);
    damage.setFlaws(storage, material, context);
    auto flags = DamageFlag::PRESSURE | DamageFlag::STRESS_TENSOR | DamageFlag::REDUCTION_FACTOR;
    REQUIRE_NOTHROW(damage.reduce(storage, flags, material));
    REQUIRE_NOTHROW(damage.integrate(storage, material));

    /// \todo check that if the strain if below eps_min, damage wont increase
}

TEST_CASE("Damage stress modification", "[damage]") {}
