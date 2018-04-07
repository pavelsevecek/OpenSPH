#include "sph/equations/Heat.h"
#include "catch.hpp"
#include "tests/Setup.h"

using namespace Sph;

TEST_CASE("Heat Diffusion", "[heat]") {
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, 10._f).set(BodySettingsId::ENERGY, 10._f);
    Storage storage = Tests::getGassStorage(100, settings);
    HeatDiffusionEquation eq;
    REQUIRE_NOTHROW(eq.create(storage, storage.getMaterial(0)));
    REQUIRE_NOTHROW(eq.initialize(storage, ThreadPool::getGlobalInstance()));
    REQUIRE_NOTHROW(eq.finalize(storage, ThreadPool::getGlobalInstance()));

    /// \todo more tests
}
