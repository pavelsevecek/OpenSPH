#include "system/Logger.h"
#include "catch.hpp"
#include "io/FileSystem.h"
#include <regex>
#include <thread>

using namespace Sph;

TEST_CASE("StdOutLogger", "[logger]") {
    REQUIRE_NOTHROW(StdOutLogger().write("stdout logger", 123, 4.f, "text"));
}

TEST_CASE("FileLogger", "[logger]") {
    {
        FileLogger logger("log1.txt");
        REQUIRE_THROWS(FileLogger("log1.txt")); // file is locked and cannot be used by other logger
        logger.write("first line");
    }
    std::string content = readFile("log1.txt");
    REQUIRE(content == "first line");

    {
        FileLogger logger("log1.txt", FileLogger::Options::APPEND);
        logger.write("second line");
    }
    content = readFile("log1.txt");
    REQUIRE(content == "first line\nsecond line");

    {
        FileLogger logger("log1.txt");
        logger.write("file cleared");
    }
    content = readFile("log1.txt");
    REQUIRE(content == "file cleared");
}

TEST_CASE("FileLogger timestamp", "[logger]") {
    {
        FileLogger logger("log2.txt", FileLogger::Options::ADD_TIMESTAMP);
        logger.write("hello world");
    }
    std::string content = readFile("log2.txt");
    REQUIRE(!content.empty());
    REQUIRE(content.find("hello world") != std::string::npos);

    std::regex regex("[A-Z][a-z][a-z] [0-3][0-9], [0-2][0-9]:[0-5][0-9]:[0-5][0-9]");
    std::smatch match;

    REQUIRE(std::regex_match(content, match, regex));
}

TEST_CASE("FileLogger Open when writing", "[logger]") {
    FileLogger logger("log3.txt", FileLogger::Options::OPEN_WHEN_WRITING);
    REQUIRE_NOTHROW(logger.write("first line"));
    std::string content;
    REQUIRE_NOTHROW(content = readFile("log3.txt"));
    REQUIRE(content == "first line");
    REQUIRE_NOTHROW(logger.write("second line"));
    REQUIRE_NOTHROW(content = readFile("log3.txt"));
    REQUIRE(content == "first line\nsecond line");
}
