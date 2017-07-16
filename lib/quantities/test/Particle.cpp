#include "quantities/Particle.h"
#include "catch.hpp"
#include "tests/Setup.h"

using namespace Sph;

TEST_CASE("Particle from storage", "[particle]") {
    Storage storage = Tests::getGassStorage(100);
    Particle p1(storage, 0);

    /// \todo
}
