#include "io/Path.h"
#include "catch.hpp"
#include "utils/Config.h"

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
    REQUIRE(Path("archive.tar.gz").extension() == Path("gz"));
    REQUIRE(Path(".gitignore").extension() == Path());
}

TEST_CASE("Path replaceExtension", "[path]") {
    REQUIRE(Path().replaceExtension("tmp") == Path());
    REQUIRE(Path("/").replaceExtension("tmp") == Path("/"));
    REQUIRE(Path("/usr/.").replaceExtension("tmp") == Path("/usr/."));
    REQUIRE(Path("/usr/file").replaceExtension("tmp") == Path("/usr/file.tmp"));
    REQUIRE(Path("/usr/file.tar.gz").replaceExtension("zip") == Path("/usr/file.tar.zip"));
    REQUIRE(Path("/usr/file.").replaceExtension("tmp") == Path("/usr/file.tmp"));
    REQUIRE(Path("/usr/.gitignore").replaceExtension("tmp") == Path("/usr/.gitignore.tmp"));
    REQUIRE(Path("/usr/local/..").replaceExtension("tmp") == Path("/usr/local/.."));
    REQUIRE(Path("file").replaceExtension("") == Path("file"));
    REQUIRE(Path("file.txt").replaceExtension("") == Path("file"));
    REQUIRE(Path("/usr/file.txt").replaceExtension("") == Path("/usr/file"));
    REQUIRE(Path("/usr/./test/../file.txt").replaceExtension("") == Path("/usr/./test/../file"));
}

TEST_CASE("Path removeExtension", "[path]") {
    REQUIRE(Path().removeExtension() == Path());
    REQUIRE(Path("/").removeExtension() == Path("/"));
    REQUIRE(Path("/usr/.").removeExtension() == Path("/usr/."));
    REQUIRE(Path("/usr/file").removeExtension() == Path("/usr/file"));
    REQUIRE(Path("/usr/file.tar.gz").removeExtension() == Path("/usr/file.tar"));
    REQUIRE(Path("/usr/file.gz").removeExtension() == Path("/usr/file"));
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

TEST_CASE("Path isAbsolute", "[path]") {
#ifndef SPH_WIN
    REQUIRE(Path("/usr/lib").isAbsolute());
    REQUIRE(Path("/etc/").isAbsolute());
#else
    REQUIRE(Path("C:/Windows").isAbsolute());
    REQUIRE(Path("D:/").isAbsolute());
#endif
    REQUIRE_FALSE(Path("file.txt").isAbsolute());
    REQUIRE_FALSE(Path("../dir/file").isAbsolute());
}

TEST_CASE("Path makeAbsolute", "[path]") {
    REQUIRE(Path().makeAbsolute() == Path());

#ifndef SPH_WIN
    REQUIRE(Path("/").makeAbsolute() == Path("/"));
    REQUIRE(Path("/usr/lib/").makeAbsolute() == Path("/usr/lib/"));
#else
    REQUIRE(Path("A:/").makeAbsolute() == Path("A:/"));
    REQUIRE(Path("C:/Windows").makeAbsolute() == Path("C:/Windows"));
#endif
    REQUIRE(Path("file").makeAbsolute() == WORKING_DIR / Path("file"));
    REQUIRE(Path("./file").makeAbsolute() == WORKING_DIR / Path("file"));
    REQUIRE(Path(".").makeAbsolute() == WORKING_DIR);
    REQUIRE(Path("../../file").makeAbsolute() == WORKING_DIR.parentPath().parentPath() / Path("file"));
}

TEST_CASE("Path makeRelative", "[path]") {
    REQUIRE(Path().makeRelative() == Path());
    REQUIRE(Path(".").makeRelative() == Path("."));
    REQUIRE(Path("file/file").makeRelative() == Path("file/file"));
    REQUIRE((WORKING_DIR / Path("file")).makeRelative() == Path("file"));
    REQUIRE(Path("file").makeAbsolute().makeRelative() == Path("file"));
    REQUIRE(WORKING_DIR.parentPath().parentPath().parentPath().makeRelative() == Path("../../.."));

    Path path = WORKING_DIR.parentPath().parentPath();
    REQUIRE(path.makeRelative().makeAbsolute() == path);
}

TEST_CASE("Path string", "[path]") {
    REQUIRE(Path().string() == "");

#ifndef SPH_WIN
    REQUIRE(Path("/").string() == "/");
    REQUIRE(Path("\\").string() == "/");
    REQUIRE(Path("/usr\\\\local////test").string() == "/usr/local/test");
#else
    REQUIRE(Path("/").native() == "\\");
    REQUIRE(Path("\\").native() == "\\");
    REQUIRE(Path("C:/Windows\\Users").native() == "C:\\Windows\\Users");
#endif
}

TEST_CASE("Current path", "[path]") {
    Path path = Path::currentPath();

    REQUIRE(path == WORKING_DIR);
}
