#include "sph/equations/av/Stress.h"
#include "catch.hpp"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"
#include "utils/Setup.h"

using namespace Sph;

TEST_CASE("StressAV nothrow", "[av]") {
    Storage storage = Tests::getGassStorage(1000);
    StressAV as(RunSettings::getDefaults());
    as.create(storage, storage.getMaterial(0));
    as.initialize(storage);
    as.finalize(storage);
}
