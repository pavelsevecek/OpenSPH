#include "objects/wrappers/Lockable.h"
#include "catch.hpp"
#include "utils/RecordType.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Lockable default construct", "[lockable]") {
    RecordType::resetStats();
    Lockable<RecordType> l;
    REQUIRE(RecordType::constructedNum == 1);
    REQUIRE(l->wasDefaultConstructed);
    auto proxy = l.lock();
    proxy->value = 5;

    REQUIRE_ASSERT(l.lock());
    REQUIRE_ASSERT(l->wasDefaultConstructed);

    proxy.release();
    REQUIRE(l->value == 5);
}

TEST_CASE("Lockable move", "[lockable]") {
    RecordType::resetStats();
    Lockable<RecordType> l;
}

TEST_CASE("Lockable assign", "[lockable]") {
    Lockable<RecordType> l;
    l = RecordType(5);
}

TEST_CASE("Lockable expire locked", "[lockable]") {
    Lockable<RecordType> l;
    auto ptr = l.lock();
}
