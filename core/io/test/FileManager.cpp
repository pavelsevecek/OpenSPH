#include "io/FileManager.h"
#include "catch.hpp"
//#include <cctype>

using namespace Sph;

TEST_CASE("UniqueFileManager getPath", "[filemanager]") {
    UniquePathManager manager;
    REQUIRE(manager.getPath(Path("path")) == Path("path"));
    REQUIRE(manager.getPath(Path("path")) == Path("path_001"));
    REQUIRE(manager.getPath(Path("path")) == Path("path_002"));

    REQUIRE(manager.getPath(Path("path.txt")) == Path("path.txt"));
    REQUIRE(manager.getPath(Path("path.txt")) == Path("path_001.txt"));
    REQUIRE(manager.getPath(Path("path.tar.gz")) == Path("path.tar.gz"));
    REQUIRE(manager.getPath(Path("path.tar.gz")) == Path("path.tar_001.gz"));

    REQUIRE(manager.getPath(Path("/absolute/path")) == Path("/absolute/path"));
    REQUIRE(manager.getPath(Path("/absolute/path")) == Path("/absolute/path_001"));

    /// \todo what to do in this cases? Detect the appendix _[0-9][0-9][0-9] ?
    REQUIRE(manager.getPath(Path("path_001")) == Path("path_001_001"));
    REQUIRE(manager.getPath(Path("path_004")) == Path("path_004"));
    REQUIRE(manager.getPath(Path("path_004")) == Path("path_004_001"));
}

TEST_CASE("UniqueFileManager exception", "[filemanager]") {
    UniquePathManager manager;
    auto generate = [&manager] {
        for (Size i = 0; i < 1000; ++i) {
            manager.getPath(Path("path"));
        }
    };
    REQUIRE_THROWS(generate());
}

TEST_CASE("RandomPathManager", "[filemanager]") {
    RandomPathManager manager;
    Path path = manager.getPath();
    REQUIRE(path.extension().empty());

    std::string name = path.native();
    REQUIRE(name.size() == 8); /// \todo make variable?
    REQUIRE(std::find_if_not(name.begin(), name.end(), [](char c) { return std::isalnum(c); }) == name.end());

    path = manager.getPath("txt");
    REQUIRE(path.extension() == Path("txt"));
    REQUIRE(path.removeExtension().native().size() == 8);

    for (Size i = 0; i < 5; ++i) {
        REQUIRE(manager.getPath() != manager.getPath());
    }
}
