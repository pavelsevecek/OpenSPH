#include "io/Logger.h"
#include "catch.hpp"
#include "io/FileSystem.h"
#include "utils/Utils.h"
#include <regex>


using namespace Sph;

TEST_CASE("StdOutLogger", "[logger]") {
    REQUIRE_NOTHROW(StdOutLogger().write("stdout logger", 123, 4.f, "text"));
    StdOutLogger().write("default",
        Console::Foreground::GREEN,
        "green",
        Console::Background::RED,
        "red",
        Console::Background::DEFAULT,
        Console::Foreground::DEFAULT);
}

TEST_CASE("FileLogger", "[logger]") {
    {
        FileLogger logger(Path("log1.txt"));
        // file is locked and cannot be used by other logger
        // REQUIRE_THROWS(FileLogger("log1.txt"));
        logger.write("first line");
    }
    std::string content = FileSystem::readFile(Path("log1.txt"));
    REQUIRE(content == "first line\n");

    {
        FileLogger logger(Path("log1.txt"), FileLogger::Options::APPEND);
        logger.write("second line");
    }
    content = FileSystem::readFile(Path("log1.txt"));
    REQUIRE(content == "first line\nsecond line\n");

    {
        FileLogger logger(Path("log1.txt"));
        logger.write("file cleared");
    }
    content = FileSystem::readFile(Path("log1.txt"));
    REQUIRE(content == "file cleared\n");
}

TEST_CASE("FileLogger timestamp", "[logger]") {
    {
        FileLogger logger(Path("log2.txt"), FileLogger::Options::ADD_TIMESTAMP);
        logger.write("hello world");
    }
    std::string content = FileSystem::readFile(Path("log2.txt"));
    REQUIRE(!content.empty());
    REQUIRE(content.find("hello world") != std::string::npos);

    std::regex regex("[A-Z][a-z][a-z] [0-3][0-9], [0-2][0-9]:[0-5][0-9]:[0-5][0-9]");
    std::smatch match;

    std::string s = content.c_str();
    REQUIRE(std::regex_search(s, match, regex));
}

TEST_CASE("FileLogger Open when writing", "[logger]") {
    FileSystem::removePath(Path("log3.txt"));
    FileLogger logger(Path("log3.txt"), FileLogger::Options::OPEN_WHEN_WRITING);
    REQUIRE_NOTHROW(logger.write("first line"));
    std::string content;
    REQUIRE_NOTHROW(content = FileSystem::readFile(Path("log3.txt")));
    REQUIRE(content == "first line\n");
    REQUIRE_NOTHROW(logger.write("second line"));
    REQUIRE_NOTHROW(content = FileSystem::readFile(Path("log3.txt")));
    REQUIRE(content == "first line\nsecond line\n");
}