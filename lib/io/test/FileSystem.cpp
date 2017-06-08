#include "io/FileSystem.h"
#include "catch.hpp"
#include "math/rng/Rng.h"
#include "utils/Utils.h"
#include <fstream>
#include <regex>

using namespace Sph;

static std::string getRandomName() {
    static char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    UniformRng rng(time(NULL));
    std::string name;
    for (Size i = 0; i < 8; ++i) {
        // -1 for terminating zero, -2 is the last indexable position
        const Size index = min(Size(rng() * (sizeof(chars) - 1)), Size(sizeof(chars) - 2));
        name += chars[index];
    }
    return name;
}

class TestFile {
private:
    Path path;

public:
    TestFile() {
        path = Path(getRandomName() + ".tmp");
        std::ofstream ofs(path.native());
    }

    ~TestFile() {
        remove(path.native().c_str());
    }

    operator Path() const {
        return path;
    }
};

class TestDirectory {
private:
    Path path;

public:
    TestDirectory() {
        path = Path(getRandomName());
        createDirectory(path);
    }

    ~TestDirectory() {
        removeDirectory(path);
    }

    operator Path() const {
        return path;
    }
};

TEST_CASE("Path exists", "[filesystem]") {
    TestFile file;
    REQUIRE(pathExists(file));
    REQUIRE(pathExists(Path(file).makeAbsolute()));
    REQUIRE_FALSE(pathExists(Path(file).removeExtension())); // extension is relevant
    REQUIRE_FALSE(pathExists(Path("dummy")));
}

TEST_CASE("Create and remove directory", "[filesystem]") {
    Path dummyPath("dummyDir");
    REQUIRE(createDirectory(dummyPath) == SUCCESS);
    REQUIRE(createDirectory(dummyPath) == SUCCESS); // should not fail if the directory already exists
    REQUIRE_FALSE(createDirectory(dummyPath, EMPTY_FLAGS));
    REQUIRE(pathExists(dummyPath));
    REQUIRE(createDirectory(Path("dummyDir1/dummyDir2")) == SUCCESS);

    REQUIRE(removeDirectory(dummyPath));
    REQUIRE_FALSE(pathExists(dummyPath));
    REQUIRE(removeDirectory(Path("dummyDir1")));
}
