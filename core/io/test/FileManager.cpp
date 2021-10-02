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

    REQUIRE(manager.getPath(Path(L"path\u03B1/file\u03B2.txt")) == Path(L"path\u03B1/file\u03B2.txt"));
    REQUIRE(manager.getPath(Path(L"path\u03B1/file\u03B2.txt")) == Path(L"path\u03B1/file\u03B2_001.txt"));

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

    String name = path.string();
    REQUIRE(name.size() == 8); /// \todo make variable?
    REQUIRE(
        std::find_if_not(name.begin(), name.end(), [](wchar_t c) { return std::isalnum(c); }) == name.end());

    path = manager.getPath("txt");
    REQUIRE(path.extension() == Path("txt"));
    REQUIRE(path.removeExtension().string().size() == 8);

    for (Size i = 0; i < 5; ++i) {
        REQUIRE(manager.getPath() != manager.getPath());
    }
}

TEST_CASE("Unique name manager", "[filemanager]") {
    UniqueNameManager mgr;
    REQUIRE(mgr.getName("name") == "name");
    REQUIRE(mgr.getName("name") == "name (1)");
    REQUIRE(mgr.getName("name") == "name (2)");
    REQUIRE(mgr.getName("test") == "test");
    REQUIRE(mgr.getName("test") == "test (1)");
    REQUIRE(mgr.getName(L"name\u03B2") == L"name\u03B2");
    REQUIRE(mgr.getName(L"name\u03B2") == L"name\u03B2 (1)");
}
