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

    // Avoid deleting (and possibly asserting on fail) the file in destructor
    void markDeleted() {
        path = Path();
    }

    ~TestFile() {
        if (!path.empty()) {
            Outcome result = FileSystem::removePath(path);
            ASSERT(result);
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
        FileSystem::createDirectory(path);
    }

    ~TestDirectory() {
        Outcome result = FileSystem::removePath(path, FileSystem::RemovePathFlag::RECURSIVE);
        ASSERT(result);
    }

    operator Path() const {
        return path;
    }
};

TEST_CASE("PathExists", "[filesystem]") {
    TestFile file;
    REQUIRE(FileSystem::pathExists(file));
    REQUIRE(FileSystem::pathExists(Path(file).makeAbsolute()));
    REQUIRE_FALSE(FileSystem::pathExists(Path(file).removeExtension())); // extension is relevant
    REQUIRE_FALSE(FileSystem::pathExists(Path("dummy")));
}

TEST_CASE("PathType", "[filesystem]") {
    TestFile file;
    TestDirectory directory;
    REQUIRE(FileSystem::getPathType(file).valueOr(FileSystem::PathType::OTHER) == FileSystem::PathType::FILE);
    REQUIRE(FileSystem::getPathType(directory).valueOr(FileSystem::PathType::OTHER) ==
            FileSystem::PathType::directory);
    REQUIRE_FALSE(FileSystem::getPathType("123456789"));
}

TEST_CASE("RemovePath", "[filesystem]") {
    REQUIRE_FALSE(FileSystem::removePath(Path("")));
    REQUIRE_FALSE(FileSystem::removePath(Path("fdsafdqfqffqfdq")));
    TestFile file;
    REQUIRE(FileSystem::removePath(file));
    file.markDeleted();
}

TEST_CASE("SetWorkingDirectory", "[filesystem]") {
    const Path current = Path::currentPath();
    const Path newPath = Path("/home/pavel/");
    FileSystem::setWorkingDirectory(newPath);
    REQUIRE(Path::currentPath() == newPath);

    FileSystem::setWorkingDirectory(current);
    REQUIRE(Path::currentPath() == current);
}

TEST_CASE("DirectoryIterator", "[filesystem]") {
    TestDirectory dir;
    Array<TestFile> files;
    for (Size i = 0; i < 5; ++i) {
        files.emplaceBack(dir);
    }
    Size found = 0;
    for (Path path : FileSystem::iterateDirectory(dir)) {
        REQUIRE(std::find_if(files.begin(), files.end(), [&path, &dir](TestFile& file) {
            return Path(file) == Path(dir) / path;
        }) != files.end());
        found++;
    }
    REQUIRE(found == 5);
}

TEST_CASE("Create and remove directory", "[filesystem]") {
    Path dummyPath("dummyDir");
    REQUIRE(FileSystem::createDirectory(dummyPath) == SUCCESS);
    // should not fail if the directory already exists
    REQUIRE(FileSystem::createDirectory(dummyPath) == SUCCESS);
    REQUIRE_FALSE(FileSystem::createDirectory(dummyPath, EMPTY_FLAGS));
    REQUIRE(FileSystem::pathExists(dummyPath));
    REQUIRE(FileSystem::createDirectory(Path("dummyDir1/dummyDir2")) == SUCCESS);

    REQUIRE(FileSystem::removePath(dummyPath));
    REQUIRE_FALSE(FileSystem::pathExists(dummyPath));
    REQUIRE_FALSE(FileSystem::removePath(Path("dummyDir1")));
    REQUIRE(FileSystem::removePath(Path("dummyDir1"), FileSystem::RemovePathFlag::RECURSIVE));
}
