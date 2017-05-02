#include "sph/equations/heat/HeatDiffusion.h"
#include "catch.hpp"
#include "utils/Setup.h"

#include "io/Logger.h"

using namespace Sph;

TEST_CASE("Heat Diffusion", "[heat]") {
    Storage storage = Tests::getGassStorage(100);
    HeatDiffusionEquation eq;
    REQUIRE_NOTHROW(eq.create(storage, storage.getMaterial(0)));
    REQUIRE_NOTHROW(eq.initialize(storage));
    REQUIRE_NOTHROW(eq.finalize(storage));
}
