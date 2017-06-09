#include "io/FileSystem.h"
#include "catch.hpp"
#include "math/rng/Rng.h"
#include "utils/Utils.h"
#include <fstream>
#include <regex>

using namespace Sph;

static std::string getRandomName() {
    static UniformRng rng(time(NULL));
    static char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
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
    TestFile(const Path& parentDir = Path()) {
        path = parentDir / Path(getRandomName() + ".tmp");
        std::ofstream ofs(path.native());
    }

    TestFile(TestFile&& other)
        : path(std::move(other.path)) {
        other.path = Path();
    }

    ~TestFile() {
        if (!path.empty()) {
            removePath(path);
        }
    }

    operator Path() const {
        return path;
    }
};

class TestDirectory {
private:
    Path path;

public:
    TestDirectory(const Path& parentDir = Path()) {
        path = parentDir / Path(getRandomName());
        createDirectory(path);
    }

    ~TestDirectory() {
        removePath(path);
    }

    operator Path() const {
        return path;
    }
};

TEST_CASE("PathExists", "[filesystem]") {
    TestFile file;
    REQUIRE(pathExists(file));
    REQUIRE(pathExists(Path(file).makeAbsolute()));
    REQUIRE_FALSE(pathExists(Path(file).removeExtension())); // extension is relevant
    REQUIRE_FALSE(pathExists(Path("dummy")));
}

TEST_CASE("RemovePath", "[filesystem]") {
    REQUIRE_FALSE(removePath(Path("")));
    REQUIRE_FALSE(removePath(Path("fdsafdqfqffqfdq")));
    TestFile file;
    REQUIRE(removePath(file));
}

TEST_CASE("DirectoryIterator", "[filesystem]") {
    TestDirectory dir;
    Array<TestFile> files;
    for (Size i = 0; i < 5; ++i) {
        files.emplaceBack(dir);
    }
    Size found = 0;
    for (Path path : iterateDirectory(dir)) {
        REQUIRE(std::find_if(files.begin(), files.end(), [&path, &dir](TestFile& file) {
            return Path(file) == Path(dir) / path;
        }) != files.end());
        found++;
    }
    REQUIRE(found == 5);
}

TEST_CASE("Create and remove directory", "[filesystem]") {
    Path dummyPath("dummyDir");
    REQUIRE(createDirectory(dummyPath) == SUCCESS);
    REQUIRE(createDirectory(dummyPath) == SUCCESS); // should not fail if the directory already exists
    REQUIRE_FALSE(createDirectory(dummyPath, EMPTY_FLAGS));
    REQUIRE(pathExists(dummyPath));
    REQUIRE(createDirectory(Path("dummyDir1/dummyDir2")) == SUCCESS);

    REQUIRE(removePath(dummyPath));
    REQUIRE_FALSE(pathExists(dummyPath));
    REQUIRE_FALSE(removePath(Path("dummyDir1")));
    //   REQUIRE(removePath(Path("dummyDir1"), RemovePathFlag::RECURSIVE));
}
