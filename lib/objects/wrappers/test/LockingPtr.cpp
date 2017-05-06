#include "objects/wrappers/LockingPtr.h"
#include "catch.hpp"
#include "utils/RecordType.h"
#include "utils/Utils.h"
#include <thread>

using namespace Sph;

TEST_CASE("LockingPtr default construct", "[lockingptr]") {
    RecordType::resetStats();
    LockingPtr<RecordType> l;
    REQUIRE_ASSERT(l->value);
    REQUIRE(RecordType::constructedNum == 0);
    auto proxy = l.lock();
    REQUIRE(proxy.get() == nullptr);
    REQUIRE_ASSERT(proxy->value);
    REQUIRE_FALSE(proxy.isLocked());
    REQUIRE_NOTHROW(proxy.release());
}

TEST_CASE("LockingPtr ptr construct", "[lockingptr]") {
    RecordType::resetStats();
    {
        LockingPtr<RecordType> l(new RecordType(5));
        REQUIRE(RecordType::constructedNum == 1);
        REQUIRE(l->value == 5);
        l->value = 7;
        REQUIRE(l->value == 7);

        auto proxy = l.lock();
        REQUIRE(proxy->value == 7);
        REQUIRE((*proxy).value == 7);
    }
    REQUIRE(RecordType::destructedNum == 1);
}

TEST_CASE("LockingPtr copy construct", "[lockingptr]") {
    RecordType::resetStats();
    LockingPtr<RecordType> l1(new RecordType(5));
    {
        LockingPtr<RecordType> l2(l1);
        REQUIRE(l2->value == 5);
    }
    REQUIRE(l1->value == 5);
    REQUIRE(RecordType::destructedNum == 0);
    l1.reset();
    REQUIRE(RecordType::destructedNum == 1);
}

TEST_CASE("LockingPtr concurrent access", "[lockingptr]") {
    LockingPtr<RecordType> l1(new RecordType(5));
    LockingPtr<RecordType> l2(l1);
    std::thread t = std::thread([&l1] {
        auto proxy = l1.lock();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        proxy->value = 8;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    l2->value = 5; // cannot assign immediatelly, already locked by l1
    REQUIRE(l2->value == 5);
    t.join();
}

TEST_CASE("LockingPtr reset while locked", "[lockingptr]") {
    LockingPtr<RecordType> l1(new RecordType(5));
    bool valueSet = false;
    std::thread t = std::thread([&l1, &valueSet] {
        auto proxy = l1.lock();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        proxy->value = 8;
        valueSet = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    REQUIRE_NOTHROW(l1.reset());
    REQUIRE(!l1);
    REQUIRE(valueSet);
    t.join();
}
