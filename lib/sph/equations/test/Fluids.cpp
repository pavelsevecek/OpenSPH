#include "sph/equations/Fluids.h"
#include "catch.hpp"
#include "tests/Setup.h"

using namespace Sph;

TEST_CASE("Fluids", "[fluids]") {
    Storage storage = Tests::getGassStorage(100);
    CohesionTerm term;
    REQUIRE_NOTHROW(term.create(storage, storage.getMaterial(0)));
    REQUIRE_NOTHROW(term.initialize(storage, ThreadPool::getGlobalInstance()));
    REQUIRE_NOTHROW(term.finalize(storage, ThreadPool::getGlobalInstance()));
}
