#include "io/OutOfCore.h"
#include "catch.hpp"
#include "io/FileManager.h"

using namespace Sph;

static_assert(sizeof(DiskArray<int>) == sizeof(Path), "DiskArray should only store the path, not the data");

TEST_CASE("DiskArray empty", "[outofcore]") {
    RandomPathManager manager;
    DiskArray<int> data(manager.getPath());
    REQUIRE(data.size() == 0);
    REQUIRE(data.empty());
}

TEST_CASE("DiskArray push", "[outofcore]") {
    RandomPathManager manager;
    DiskArray<int> data(manager.getPath());
    data.push(5);
    REQUIRE(data.size() == 1);
    REQUIRE_FALSE(data.empty());
    data.push(3);
    REQUIRE(data.size() == 2);

    REQUIRE(data[0] == 5);
    REQUIRE(data[1] == 3);
    REQUIRE_THROWS(data[2]);
}

TEST_CASE("DiskArray getAll", "[outofcore]") {
    RandomPathManager manager;
    DiskArray<int> data(manager.getPath());
    REQUIRE(data.getAll().empty());

    data.push(1);
    data.push(2);
    data.push(3);
    Array<int> all = data.getAll();
    REQUIRE(all.size() == 3);
    REQUIRE(all[0] == 1);
    REQUIRE(all[1] == 2);
    REQUIRE(all[2] == 3);
}

TEST_CASE("DiskArray clear", "[outofcore]") {
    RandomPathManager manager;
    Path path = manager.getPath();
    DiskArray<int> data(path);
    data.push(5);
    data.push(7);
    REQUIRE(FileSystem::pathExists(path));
    data.clear();
    REQUIRE_FALSE(FileSystem::pathExists(path));
    REQUIRE_NOTHROW(data.clear());
}
