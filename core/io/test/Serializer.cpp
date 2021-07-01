#include "io/Serializer.h"
#include "catch.hpp"
#include "io/FileSystem.h"

using namespace Sph;

TEST_CASE("Serialize", "[serialize]") {
    Path path("serialized");
    {
        Serializer<true> serializer(path);
        serializer.write(5, 5u, 'c', 3.f, 4., "SPH");
        serializer.addPadding(13);
        serializer.write(std::string("test"));
    }
    REQUIRE(FileSystem::fileSize(path) == 44 + 13 + 5);

    {
        Deserializer<true> deserializer(path);
        Size p1;
        int64_t p2;
        int p3;
        REQUIRE_NOTHROW(deserializer.read(p1, p2, p3));
        REQUIRE(p1 == 5);
        REQUIRE(p2 == 5);
        REQUIRE(p3 == 'c');
        float f;
        double d;
        REQUIRE_NOTHROW(deserializer.read(f, d));
        REQUIRE(f == 3.f);
        REQUIRE(d == 4.);
        std::string s;
        REQUIRE_NOTHROW(deserializer.read(s));
        REQUIRE(s == "SPH");

        REQUIRE_NOTHROW(deserializer.skip(13));
        REQUIRE_NOTHROW(deserializer.read(s));
        REQUIRE(s == "test");

        REQUIRE_THROWS(deserializer.read(p1));
        REQUIRE_THROWS(deserializer.read(s));
    }
}
