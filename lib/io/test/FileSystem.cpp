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
    std::string name;

public:
    TestFile() {
        name = getRandomName() + ".tmp";
        std::ofstream ofs(name);
    }

    ~TestFile() {
        remove(name.c_str());
    }

    operator std::string() const {
        return name;
    }
};

class TestDirectory {
private:
    std::string name;

public:
    TestDirectory() {
        name = getRandomName();
        createDirectory(name);
    }

    ~TestDirectory() {
        removeDirectory(name);
    }

    operator std::string() const {
        return name;
    }
};

TEST_CASE("File exists", "[filesystem]") {
    TestFile file;
    REQUIRE(fileExists(file));
    REQUIRE_FALSE(fileExists(std::string(file).substr(0, 8))); // extension is relevant
    REQUIRE_FALSE(fileExists("dummy"));
}

TEST_CASE("Create and remove directory", "[filesystem]") {
    REQUIRE(createDirectory("dummyDir") == SUCCESS);
    REQUIRE(createDirectory("dummyDir") == SUCCESS); // should not fail if the directory already exists
    REQUIRE_FALSE(createDirectory("dummyDir", EMPTY_FLAGS));
    REQUIRE(fileExists("dummyDir"));
    /// \todo REQUIRE(createDirectory("dummyDir1/dummyDir2") == SUCCESS);

    REQUIRE(removeDirectory("dummyDir"));
    REQUIRE_FALSE(fileExists("dummyDir"));
    /// \todo REQUIRE(removeDirectory("dummyDir1"));
}
