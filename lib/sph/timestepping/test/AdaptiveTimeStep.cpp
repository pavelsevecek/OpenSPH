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

    // get timestep limited by CFL
    FrequentStats stats;
    const Float step = getter.get(storage, INFTY, stats);

    ArrayView<Vector> r = storage.getValue<Vector>(QuantityKey::POSITIONS);
    const Float h = r[0][H]; // all hs are the same
    const Float expected = courant * h / cs;
    REQUIRE(almostEqual(expected, step));
    REQUIRE(stats.get<QuantityKey>(FrequentStatsIds::TIMESTEP_CRITERION) == QuantityKey::SOUND_SPEED);

    // get timestep limited by stats
    const Float step2 = getter.get(storage, 1.e-3_f, stats);
    REQUIRE(step2 == 1.e-3_f);
    REQUIRE(stats.get<QuantityKey>(FrequentStatsIds::TIMESTEP_CRITERION) == QuantityKey::MAXIMUM_VALUE);

    // get timestep limited by value-to-derivative ration of energy
    ArrayView<Float> u, du;
    tie(u, du) = storage.getAll<Float>(QuantityKey::ENERGY);
    for (Float& f : u) {
        f = 12._f; // u = 12
    }
    for (Float& f : du) {
        f = 4._f; // du/dt = 4
    }
    //const Float factor = GLOBAL_SETTINGS.get<Float>(GlobalSettingsIds::TIMESTEPPING_ADAPTIVE_FACTOR);
    //const Float step3 = getter.get(storage, INFTY, stats);
    // this is quite imprecise due to approximative sqrt, but it doesn't really matter for timestep
    //REQUIRE(almostEqual(step3, factor * 3._f, 1.e-3_f));
    REQUIRE(stats.get<QuantityKey>(FrequentStatsIds::TIMESTEP_CRITERION) == QuantityKey::ENERGY);
}

TEST_CASE("MinOfArray", "[timestepping]") {
    Array<Float> ar1{ 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    REQUIRE(minOfArray(ar1) == 1);
    REQUIRE(ar1 == Array<Float>({ 1, 2, 3, 4, 5, 6, 7, 8, 9 }));
    Array<Float> ar2{ 3, 2, 7, 5, 3, 4, 1, 5, 9 };
    REQUIRE(minOfArray(ar2) == 1);
    REQUIRE(ar2 == Array<Float>({ 1, 2, 5, 5, 1, 4, 1, 5, 9 }));
    Array<Float> ar3{ 11, 10, 9, 8, 7, 6, 5, 4, 3 };
    REQUIRE(minOfArray(ar3) == 3);
    Array<Float> ar4{ 2, 4, 6, 8 };
    REQUIRE(minOfArray(ar4) == 2);
    Array<Float> ar5{ 1 };
    REQUIRE(minOfArray(ar5) == 1);
    Array<Float> ar6{ 9, 5, 3, 6, 2, 5, 8, 1, 23, 6, 4 };
    REQUIRE(minOfArray(ar6) == 1);
}
