#include "sph/timestepping/AdaptiveTimeStep.h"
#include "catch.hpp"
#include "quantities/Storage.h"
#include "sph/initial/Distribution.h"

using namespace Sph;
TEST_CASE("Adaptive Timestep", "[timestepping]") {
    AdaptiveTimeStep getter(GLOBAL_SETTINGS);
    const Float courant = GLOBAL_SETTINGS.get<Float>(GlobalSettingsIds::TIMESTEPPING_COURANT);
    Storage storage;
    HexagonalPacking distribution;
    storage.emplace<Vector, OrderEnum::SECOND_ORDER>(
        QuantityKey::POSITIONS, distribution.generate(100, BlockDomain(Vector(0._f), Vector(100._f))));
    storage.emplace<Float, OrderEnum::FIRST_ORDER>(QuantityKey::ENERGY, 0._f);

    const Float cs = 5._f;
    storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::SOUND_SPEED, cs);
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityKey::POSITIONS);
    const Float h = r[0][H]; // all hs are the same
    const Float step = getter.get(storage, INFTY);
    const Float expected = courant * h / cs;
    REQUIRE(Math::almostEqual(expected, step));

    const Float step2 = getter.get(storage, 1.e-3_f);
    REQUIRE(step2 == 1.e-3_f);

    ArrayView<Float> u, du;
    tie(u, du)=  storage.getAll<Float>(QuantityKey::ENERGY);
    for (Float& f : u) {
        f = 12._f; // u = 12
    }
    for (Float& f : du) {
        f = 4._f; // du/dt = 4
    }
    const Float factor = GLOBAL_SETTINGS.get<Float>(GlobalSettingsIds::TIMESTEPPING_ADAPTIVE_FACTOR);
    const Float step3 = getter.get(storage, INFTY);
    REQUIRE(step3 == factor * 3._f);
}
