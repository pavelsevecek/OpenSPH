#include "system/Process.h"
#include "catch.hpp"
#include "io/FileSystem.h"

using namespace Sph;

TEST_CASE("Process create", "[process]") {
    const Path expected("process.txt");
    removePath(expected); // remove from previous test, not a very good solution ...
    Process process(Path("/usr/bin/touch"), { expected.native() });
    process.wait();
    REQUIRE(pathExists(expected));
}
