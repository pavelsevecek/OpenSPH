#include "io/FileSystem.h"
#include "catch.hpp"
#include "io/FileManager.h"
#include "objects/utility/IteratorAdapters.h"
#include "system/Platform.h"
#include "utils/Config.h"
#include "utils/Utils.h"
#include <fstream>
#include <regex>

using namespace Sph;

class TestFile {
private:
    Path path;
    RandomPathManager manager;

public:
    TestFile(const Path& parentDir = Path("temp")) {
        FileSystem::createDirectory(parentDir);
        path = parentDir / manager.getPath("tmp");
        std::ofstream ofs(path.native(), std::ios::out | std::ios::binary);
    }

    TestFile(TestFile&& other)
        : path(std::move(other.path)) {
        other.path = Path();
    }

    TestFile& operator=(TestFile&& other) {
        std::swap(path, other.path);
        return *this;
    }

    // Avoid deleting (and possibly asserting on fail) the file in destructor
    void markDeleted() {
        path = Path();
    }

    // Fill with integerts from 0 to given value (exluding the value).
    void fill(const Size num) {
        std::ofstream ofs(path.native(), std::ios::out | std::ios::binary);
        for (Size i = 0; i < num; ++i) {
            ofs.write((char*)&i, sizeof(int));
        }
        ofs.close();
    }

    ~TestFile() {
        if (!path.empty()) {
            Outcome result = FileSystem::removePath(path);
            SPH_ASSERT(result);
        }
    }

    operator Path() const {
        return path;
    }
};

class TestDirectory {
private:
    Path path;
    RandomPathManager manager;

public:
    TestDirectory(const Path& parentDir = Path("temp")) {
        path = parentDir / manager.getPath();
        FileSystem::createDirectory(path);
    }

    TestDirectory(TestDirectory&& other)
        : path(std::move(other)) {
        other.path = Path();
    }

    TestDirectory& operator=(TestDirectory&& other) {
        std::swap(path, other.path);
        return *this;
    }

    ~TestDirectory() {
        if (!path.empty()) {
            Outcome result = FileSystem::removePath(path, FileSystem::RemovePathFlag::RECURSIVE);
            SPH_ASSERT(result, result.error());
        }
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
    REQUIRE(FileSystem::pathType(file).valueOr(FileSystem::PathType::OTHER) == FileSystem::PathType::FILE);
    REQUIRE(FileSystem::pathType(directory).valueOr(FileSystem::PathType::OTHER) ==
            FileSystem::PathType::DIRECTORY);
    REQUIRE_FALSE(FileSystem::pathType(Path("123456789")));
}

TEST_CASE("CopyFile", "[filesystem]") {
    TestFile file;
    file.fill(1000);
    // sanity check
    REQUIRE(FileSystem::pathExists(file));
    const std::size_t size = FileSystem::fileSize(file);
    REQUIRE(size == 1000 * sizeof(int));

    TestDirectory dir;
    RandomPathManager manager;
    const Path to = Path(dir) / manager.getPath("tmp");
    REQUIRE(FileSystem::copyFile(file, to));
    REQUIRE(FileSystem::pathExists(to));
    REQUIRE(FileSystem::pathType(to).valueOr(FileSystem::PathType::OTHER) == FileSystem::PathType::FILE);
    REQUIRE(FileSystem::fileSize(to) == size);

    // check content
    Array<int> content(1000);
    std::ifstream ifs(to.native(), std::ios::in | std::ios::binary);
    ifs.read((char*)&content[0], size);
    ifs.close();

    auto test = [&] {
        for (Size i = 0; i < content.size(); ++i) {
            if (content[i] != int(i)) {
                return false;
            }
        }
        return true;
    };
    REQUIRE(test());
}

static Size checkDirectoriesEqual(const Path& fromParent, const Path& toParent) {
    Size counter = 0;

    struct Element {
        Path from;
        Path to;
    };
    // iterate in both directories together
    // although the order of files is not guaranteed, we hope that the order will match the order of creation
    for (Element e : iterateTuple<Element>(
             FileSystem::iterateDirectory(fromParent), FileSystem::iterateDirectory(toParent))) {
        REQUIRE(e.from == e.to);
        Expected<FileSystem::PathType> fromType = FileSystem::pathType(fromParent / e.from);
        Expected<FileSystem::PathType> toType = FileSystem::pathType(toParent / e.to);
        REQUIRE(fromType);
        REQUIRE(toType);
        REQUIRE(fromType.value() == toType.value());
        if (fromType.value() == FileSystem::PathType::FILE) {
            REQUIRE(FileSystem::fileSize(fromParent / e.from) == FileSystem::fileSize(toParent / e.to));
            counter++;
        } else if (fromType.value() == FileSystem::PathType::DIRECTORY) {
            counter += checkDirectoriesEqual(fromParent / e.from, toParent / e.to);
        }
    }
    return counter;
}

TEST_CASE("CopyDirectory", "[filesystem]") {
    TestDirectory parentDir;
    // order matters as variables are destroyed in reversed order - first the files are destroyed, then the
    // parent file
    Array<TestFile> files;

    // add 5 test files to parent dir
    for (Size i = 0; i < 5; ++i) {
        files.emplaceBack(parentDir);
        files[files.size() - 1].fill(100);
    }
    // add 3 subdirectories, each containing one additional file
    Array<TestDirectory> dirs;
    for (Size i = 0; i < 3; ++i) {
        dirs.emplaceBack(Path(parentDir));
        files.emplaceBack(Path(parentDir) / Path(dirs[i]));
        files[files.size() - 1].fill(100);
    }
    TestDirectory toDir;
    REQUIRE(FileSystem::copyDirectory(parentDir, toDir));

    const Size counter = checkDirectoriesEqual(parentDir, toDir);
    // we should count 5+3=8 files in total
    REQUIRE(counter == 8);
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

/// \todo move to Platform.cpp
TEST_CASE("GetExecutablePath", "[filesystem]") {
    const Expected<Path> path = getExecutablePath();
    REQUIRE(path);
    REQUIRE(path.value() == WORKING_DIR);
}

TEST_CASE("IsPathWritable", "[filesystem]") {
    REQUIRE(FileSystem::isPathWritable(Path(".")));
    REQUIRE(FileSystem::isPathWritable(Path("/home/pavel/")));
    REQUIRE_FALSE(FileSystem::isPathWritable(Path("/usr/lib/")));
    REQUIRE_FALSE(FileSystem::isPathWritable(Path("/var/")));
    REQUIRE_FALSE(FileSystem::isPathWritable(Path("/etc/")));
}

TEST_CASE("GetHomeDirectory", "[filesystem]") {
    Expected<Path> path = FileSystem::getHomeDirectory();
    REQUIRE(path);
    REQUIRE(path.value() == Path("/home/pavel"));
}
