#include "system/Crashpad.h"
#include "catch.hpp"
#include "io/FileSystem.h"
#include "tests/Setup.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("CrashPad", "[crash]") {
    SKIP_TEST;
    SharedPtr<Storage> storage = makeShared<Storage>(Tests::getGassStorage(1000));
    // const Size particleCnt = storage->getParticleCnt();
    Path dumpPath("crashDump.ssf");
    CrashPad::setup(storage, dumpPath);
    FileSystem::removePath(dumpPath);
    REQUIRE_FALSE(FileSystem::pathExists(dumpPath));
    raise(SIGSEGV);
    REQUIRE(FileSystem::pathExists(dumpPath));
}
