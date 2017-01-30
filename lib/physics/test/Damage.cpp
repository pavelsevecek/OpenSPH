#include "physics/Damage.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "objects/containers/ArrayUtils.h"
#include "physics/Yielding.h"
#include "quantities/Storage.h"
#include "sph/initial/Distribution.h"
#include "sph/initial/Initial.h"
#include "system/ArrayStats.h"
#include "system/Settings.h"
#include "utils/Approx.h"

using namespace Sph;


TEST_CASE("Distribute flaws", "[damage]") {
    ScalarDamage model(GlobalSettings::getDefaults());
    BodySettings bodySettings;
    Storage storage(bodySettings);
    HexagonalPacking distribution;
    SphericalDomain domain(Vector(0._f), 1._f);
    Array<Vector> r = distribution.generate(9000, domain);
    const int N = r.size();
    storage.emplace<Vector, OrderEnum::SECOND_ORDER>(QuantityIds::POSITIONS, std::move(r));
    const Float rho0 = bodySettings.get<Float>(BodySettingsIds::DENSITY);
    storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityIds::DENSITY, rho0);
    storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityIds::MASSES, rho0 * domain.getVolume() / N);
    model.initialize(storage, bodySettings);

    // check that all particles have at least one flaw
    ArrayView<Size> n_flaws = storage.getValue<Size>(QuantityIds::N_FLAWS);
    REQUIRE(areAllMatching(n_flaws, [N](const int n) { return n >= 1 && n <= N; }));

    // check that the total number of flaws
    /// \todo how does this depend on N?
    int n_total = 0;
    for (int n : n_flaws) {
        n_total += n;
    }
    const Float m_weibull = bodySettings.get<Float>(BodySettingsIds::WEIBULL_EXPONENT);
    REQUIRE(n_total >= 8._f * N);
    REQUIRE(n_total <= 11._f * N);

    ArrayStats<Float> mStats(storage.getValue<Float>(QuantityIds::M_ZERO));
    ArrayStats<Float> growthStats(storage.getValue<Float>(QuantityIds::EXPLICIT_GROWTH));
    ArrayStats<Float> epsStats(storage.getValue<Float>(QuantityIds::EPS_MIN));
    REQUIRE(mStats.min() == 1._f);
    REQUIRE(mStats.max() > m_weibull);
    REQUIRE(mStats.average() == approx(m_weibull, 0.05_f));
    REQUIRE(growthStats.min() == growthStats.max());
    REQUIRE(epsStats.min() > 0._f);
    REQUIRE(epsStats.max() == approx(3.e-4_f, 0.2_f));
}

TEST_CASE("Fracture growth", "[damage]") {
    /// \todo some better test, for now just testing that integrate will work without asserts
    ScalarDamage damage(GlobalSettings::getDefaults());
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, GlobalSettings::getDefaults());
    conds.addBody(SphericalDomain(Vector(0._f), 1._f), BodySettings::getDefaults());

    damage.initialize(*storage, BodySettings::getDefaults());
    REQUIRE_NOTHROW(damage.update(*storage));
    REQUIRE_NOTHROW(damage.integrate(*storage));

    /// \todo check that if the strain if below eps_min, damage wont increase
}
