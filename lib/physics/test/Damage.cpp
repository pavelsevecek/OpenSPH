#include "physics/Damage.h"
#include "catch.hpp"
#include "objects/containers/ArrayUtils.h"
#include "physics/Yielding.h"
#include "sph/initial/Distribution.h"
#include "system/ArrayStats.h"
#include "geometry/Domain.h"

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
    REQUIRE(Range(9._f * N, 11._f * N).contains(n_total));

    ArrayStats<Float> mStats(storage.getValue<Float>(QuantityIds::M_ZERO));
    ArrayStats<Float> growthStats(storage.getValue<Float>(QuantityIds::EXPLICIT_GROWTH));
    ArrayStats<Float> epsStats(storage.getValue<Float>(QuantityIds::EPS_MIN));
    REQUIRE(mStats.min() == 1._f);
    REQUIRE(mStats.max() > m_weibull);
    REQUIRE(almostEqual(mStats.average(), m_weibull, 0.5_f));
    REQUIRE(growthStats.min() == growthStats.max());
    REQUIRE(epsStats.min() > 0._f);
    REQUIRE(almostEqual(epsStats.max(), 3.e-4_f, 1.e-4_f));
}

TEST_CASE("Fracture growth", "[damage]") {}
