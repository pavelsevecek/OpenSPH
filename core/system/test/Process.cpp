#include "system/Process.h"
#include "catch.hpp"
#include "io/FileSystem.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Process create", "[process]") {
    SKIP_TEST
    const Path expected("process.txt");
    FileSystem::removePath(expected); // remove from previous test, not a very good solution ...
    Process process(Path("/usr/bin/touch"), { expected.string() });
    process.wait();
    REQUIRE(FileSystem::pathExists(expected));
}
