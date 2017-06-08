#include "io/Path.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Empty path", "[path]") {
    Path path;
    REQUIRE(path.empty());
    REQUIRE_FALSE(path.isHidden());
    REQUIRE_FALSE(path.isRelative());
    REQUIRE_FALSE(path.isAbsolute());
    REQUIRE(path.parentPath().empty());
    REQUIRE(path.fileName().empty());
    REQUIRE(path.extension().empty());
}

TEST_CASE("Path equality", "[path]") {
    REQUIRE(Path("/usr/lib") == Path("\\usr\\lib"));
    REQUIRE(Path("/usr/lib") == Path("/usr////lib"));
    REQUIRE_FALSE(Path("/usr/lib") == Path("/ussr/lib"));
}

TEST_CASE("Path append", "[path]") {
    REQUIRE(Path("/usr/local/") / Path("share") == Path("/usr/local/share"));
    REQUIRE(Path("/usr/local") / Path("share") == Path("/usr/local/share"));
    REQUIRE(Path() / Path("/usr/local") == Path("/usr/local"));
    REQUIRE(Path() / Path("usr/local") == Path("usr/local"));
    REQUIRE(Path("/usr/local") / Path() == Path("/usr/local"));
    REQUIRE(Path() / Path() == Path());
}

TEST_CASE("Path isHidden", "[path]") {
    REQUIRE(Path(".gitignore").isHidden());
    REQUIRE(Path("/home/pavel/.gitignore").isHidden());
    REQUIRE_FALSE(Path("file").isHidden());
    REQUIRE_FALSE(Path("/home/pavel/file").isHidden());
}

TEST_CASE("Path parentPath", "[path]") {
    REQUIRE(Path("/home/pavel/file.txt").parentPath() == Path("/home/pavel/"));
    REQUIRE(Path("/home/pavel/files").parentPath() == Path("/home/pavel/"));
    REQUIRE(Path("/home/pavel/files/").parentPath() == Path("/home/pavel/"));
    REQUIRE(Path("file").parentPath() == Path());
    REQUIRE(Path("/").parentPath() == Path());
    REQUIRE(Path("/usr").parentPath() == Path("/"));
}

TEST_CASE("Path fileName", "[path]") {
    REQUIRE(Path("/home/pavel/file.txt").fileName() == Path("file.txt"));
    REQUIRE(Path("/home/pavel/files").fileName() == Path("files"));
    REQUIRE(Path("/home/pavel/files/").fileName() == Path("files"));
    REQUIRE(Path("/home").fileName() == Path("home"));
    REQUIRE(Path("file.txt").fileName() == Path("file.txt"));
    REQUIRE(Path("file").fileName() == Path("file"));
}

TEST_CASE("Path extension", "[path]") {
    REQUIRE(Path("/usr/lib").extension() == Path());
    REQUIRE(Path("/usr/lib/").extension() == Path());
    REQUIRE(Path("file.txt").extension() == Path("txt"));
    REQUIRE(Path("archive.tar.gz").extension() == Path("tar.gz"));
    REQUIRE(Path(".gitignore").extension() == Path());
}

TEST_CASE("Path replaceExtension", "[path]") {
    REQUIRE(Path().replaceExtension("tmp") == Path());
    REQUIRE(Path("/").replaceExtension("tmp") == Path("/"));
    REQUIRE(Path("/usr/.").replaceExtension("tmp") == Path("/usr/."));
    REQUIRE(Path("/usr/file").replaceExtension("tmp") == Path("/usr/file.tmp"));
    REQUIRE(Path("/usr/file.tar.gz").replaceExtension("zip") == Path("/usr/file.zip"));
    REQUIRE(Path("/usr/file.").replaceExtension("tmp") == Path("/usr/file.tmp"));
    REQUIRE(Path("/usr/.gitignore").replaceExtension("tmp") == Path("/usr/.gitignore.tmp"));
    REQUIRE(Path("/usr/local/..").replaceExtension("tmp") == Path("/usr/local/.."));
}

TEST_CASE("Path removeExtension", "[path]") {
    REQUIRE(Path().removeExtension() == Path());
    REQUIRE(Path("/").removeExtension() == Path("/"));
    REQUIRE(Path("/usr/.").removeExtension() == Path("/usr/."));
    REQUIRE(Path("/usr/file").removeExtension() == Path("/usr/file"));
    REQUIRE(Path("/usr/file.tar.gz").removeExtension() == Path("/usr/file"));
    REQUIRE(Path("/usr/file.").removeExtension() == Path("/usr/file"));
    REQUIRE(Path("/usr/.gitignore").removeExtension() == Path("/usr/.gitignore"));
    REQUIRE(Path("/usr/local/..").removeExtension() == Path("/usr/local/.."));
}

TEST_CASE("Path removeSpecialDirs", "[path]") {
    REQUIRE(Path("/usr/lib").removeSpecialDirs() == Path("/usr/lib"));
    REQUIRE(Path("./usr/lib").removeSpecialDirs() == Path("usr/lib"));
    REQUIRE(Path("././usr/lib").removeSpecialDirs() == Path("usr/lib"));
    REQUIRE(Path("/usr/lib/.").removeSpecialDirs() == Path("/usr/lib/"));
    REQUIRE(Path("/usr/lib/./.").removeSpecialDirs() == Path("/usr/lib/"));
    REQUIRE(Path("/usr/lib.").removeSpecialDirs() == Path("/usr/lib."));
    REQUIRE(Path(".usr/lib/.gitignore").removeSpecialDirs() == Path(".usr/lib/.gitignore"));
    REQUIRE(Path("/usr/./lib").removeSpecialDirs() == Path("/usr/lib"));
    REQUIRE(Path("/usr/./././lib").removeSpecialDirs() == Path("/usr/lib"));
    REQUIRE(Path(".").removeSpecialDirs() == Path());
    REQUIRE(Path("./.").removeSpecialDirs() == Path());

    REQUIRE(Path("..").removeSpecialDirs() == Path());
    REQUIRE(Path("../..").removeSpecialDirs() == Path());
    // REQUIRE(Path("../usr/lib").removeSpecialDirs() == Path("usr/lib"));
    // REQUIRE(Path("././usr/lib").removeSpecialDirs() == Path("usr/lib"));
    REQUIRE(Path("/usr/lib/..").removeSpecialDirs() == Path("/usr/"));
    REQUIRE(Path("/usr/lib/dir/../..").removeSpecialDirs() == Path("/usr/"));
    REQUIRE(Path("/usr/lib/../..").removeSpecialDirs() == Path("/"));
    REQUIRE(Path("/usr/lib..").removeSpecialDirs() == Path("/usr/lib.."));
    REQUIRE(Path("..usr/lib/..gitignore").removeSpecialDirs() == Path("..usr/lib/..gitignore"));
    REQUIRE(Path("/usr/../lib").removeSpecialDirs() == Path("/lib"));
    REQUIRE(Path("usr/../lib").removeSpecialDirs() == Path("lib"));
}

/// \todo this is fine tuned for my system, sorry ...

const Path workingDir("/home/pavel/projects/astro/sph/build/test/");

TEST_CASE("Path makeAbsolute", "[path]") {
    REQUIRE(Path().makeAbsolute() == Path());
    REQUIRE(Path("/").makeAbsolute() == Path("/"));
    REQUIRE(Path("/usr/lib/").makeAbsolute() == Path("/usr/lib/"));
    REQUIRE(Path("file").makeAbsolute() == workingDir / Path("file"));
    REQUIRE(Path("./file").makeAbsolute() == workingDir / Path("file"));
    REQUIRE(Path(".").makeAbsolute() == workingDir);
    REQUIRE(Path("../../file").makeAbsolute() == Path("/home/pavel/projects/astro/sph/file"));
}

TEST_CASE("Path makeRelative", "[path]") {
    REQUIRE(Path().makeRelative() == Path());
    REQUIRE(Path(".").makeRelative() == Path("."));
    REQUIRE(Path("file/file").makeRelative() == Path("file/file"));
    REQUIRE((workingDir / Path("file")).makeRelative() == Path("file"));
    REQUIRE(Path("/home/pavel/projects/astro/").makeRelative() == Path("../../.."));
    REQUIRE(Path("file").makeAbsolute().makeRelative() == Path("file"));
    REQUIRE(Path("/home/pavel/test").makeRelative().makeAbsolute() == Path("/home/pavel/test"));
}

TEST_CASE("Path native", "[path]") {
    REQUIRE(Path().native() == "");
    REQUIRE(Path("/").native() == "/");
    REQUIRE(Path("\\").native() == "/");
    REQUIRE(Path("/usr\\\\local////test").native() == "/usr/local/test");
}

TEST_CASE("Current path", "[path]") {
    Path path = Path::currentPath();

    REQUIRE(path == workingDir);
}
