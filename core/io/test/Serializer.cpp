#include "io/Serializer.h"
#include "catch.hpp"
#include "io/FileSystem.h"

using namespace Sph;

TEST_CASE("Serialize", "[serialize]") {
    Path path("serialized");
    {
        Serializer<true> serializer(makeAuto<FileBinaryOutputStream>(path));
        serializer.serialize(5, 5u, 'c', 3.f, 4., "SPH");
        serializer.addPadding(13);
        serializer.write(String("test"));
    }
    REQUIRE(FileSystem::fileSize(path) == 44 + 13 + 5);

    {
        Deserializer<true> deserializer(makeAuto<FileBinaryInputStream>(path));
        Size p1;
        int64_t p2;
        int p3;
        REQUIRE_NOTHROW(deserializer.deserialize(p1, p2, p3));
        REQUIRE(p1 == 5);
        REQUIRE(p2 == 5);
        REQUIRE(p3 == 'c');
        float f;
        double d;
        REQUIRE_NOTHROW(deserializer.deserialize(f, d));
        REQUIRE(f == 3.f);
        REQUIRE(d == 4.);
        String s;
        REQUIRE_NOTHROW(deserializer.deserialize(s));
        REQUIRE(s == "SPH");

        REQUIRE_NOTHROW(deserializer.skip(13));
        REQUIRE_NOTHROW(deserializer.deserialize(s));
        REQUIRE(s == "test");

        REQUIRE_THROWS(deserializer.deserialize(p1));
        REQUIRE_THROWS(deserializer.deserialize(s));
    }
}
