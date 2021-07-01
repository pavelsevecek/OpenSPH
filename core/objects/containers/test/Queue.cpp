#include "objects/containers/Queue.h"
#include "catch.hpp"
#include "utils/RecordType.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Queue default-construct", "[queue]") {
    RecordType::resetStats();
    Queue<RecordType> q;
    REQUIRE(q.size() == 0);
    REQUIRE(q.empty());
    REQUIRE(RecordType::constructedNum == 0);
    REQUIRE_ASSERT(q.front());
    REQUIRE_ASSERT(q.back());
}

TEST_CASE("Queue size construct", "[queue]") {
    RecordType::resetStats();
    Queue<RecordType> q(4);
    REQUIRE(q.size() == 4);
    REQUIRE_FALSE(q.empty());
    REQUIRE(RecordType::constructedNum == 4);
    for (Size i = 0; i < 4; ++i) {
        REQUIRE(q[i].wasDefaultConstructed);
    }
}

TEST_CASE("Queue initializer_list construct", "[queue]") {
    RecordType::resetStats();
    Queue<RecordType> q({ 1, 2, 3 });
    REQUIRE(q.size() == 3);
    REQUIRE_FALSE(q.empty());
    REQUIRE(RecordType::constructedNum == 6); // 3 temporaries
    for (Size i = 0; i < 3; ++i) {
        REQUIRE(q[i].wasCopyConstructed);
        REQUIRE(q[i].value == i + 1);
    }
    REQUIRE(q.front().value == 1);
    REQUIRE(q.back().value == 3);

    REQUIRE_ASSERT(q[3]);
}

TEST_CASE("Queue move construct", "[queue]") {
    RecordType::resetStats();
    Queue<RecordType> q1({ 4, 6, 7 });
    Queue<RecordType> q2(std::move(q1));
    REQUIRE(q1.size() == 0);
    REQUIRE(q1.empty());
    REQUIRE(q2.size() == 3);
    REQUIRE_FALSE(q2.empty());
    REQUIRE(q2[0].value == 4);
    REQUIRE(q2[1].value == 6);
    REQUIRE(q2[2].value == 7);
    for (Size i = 0; i < 3; ++i) {
        REQUIRE(q2[i].wasCopyConstructed);
        // shouldn't actually move the elements
        REQUIRE_FALSE(q2[i].wasMoveConstructed);
        REQUIRE_FALSE(q2[i].wasMoveAssigned);
    }

    REQUIRE(RecordType::constructedNum == 6);
}

TEST_CASE("Queue move assign", "[queue]") {
    RecordType::resetStats();
    Queue<RecordType> q1({ 4, 6, 7 });
    Queue<RecordType> q2({ 2, 5 });
    q2 = std::move(q1);
    REQUIRE(q1.size() == 2);
    REQUIRE_FALSE(q1.empty());
    REQUIRE(q2.size() == 3);
    REQUIRE_FALSE(q2.empty());
    REQUIRE(q2[0].value == 4);
    REQUIRE(q2[1].value == 6);
    REQUIRE(q2[2].value == 7);
    REQUIRE(q1[0].value == 2);
    REQUIRE(q1[1].value == 5);

    REQUIRE(RecordType::constructedNum == 10);
}

TEST_CASE("Queue pushBack", "[queue]") {
    RecordType::resetStats();
    Queue<RecordType> q;
    q.pushBack(5);
    REQUIRE(q.size() == 1);
    REQUIRE(q.front().value == 5);
    REQUIRE(&q.front() == &q.back());
    REQUIRE(q.front().wasDefaultConstructed);
    REQUIRE(q.front().wasCopyAssigned);

    q.pushBack(6);
    REQUIRE(q.size() == 2);
    REQUIRE(q.front().value == 5);
    REQUIRE(q[1].value == 6);

    q.pushBack(7);
    q.pushBack(8);
    q.pushBack(9);
    REQUIRE(q.size() == 5);
    for (Size i = 0; i < 5; ++i) {
        REQUIRE(((q[i].wasDefaultConstructed && q[i].wasCopyAssigned) || q[i].wasMoveConstructed));
        REQUIRE(q[i].value == 5 + i);
    }
}

TEST_CASE("Queue pushFront", "[queue]") {
    RecordType::resetStats();
    Queue<RecordType> q;
    q.pushFront(9);
    REQUIRE(q.size() == 1);
    REQUIRE(q.front().value == 9);
    REQUIRE(&q.front() == &q.back());
    REQUIRE(q.front().wasDefaultConstructed);
    REQUIRE(q.front().wasCopyAssigned);

    q.pushFront(8);
    REQUIRE(q.size() == 2);
    REQUIRE(q.front().value == 8);
    REQUIRE(q.back().value == 9);

    q.pushFront(7);
    q.pushFront(6);
    q.pushFront(5);
    REQUIRE(q.size() == 5);
    for (Size i = 0; i < 5; ++i) {
        REQUIRE(((q[i].wasDefaultConstructed && q[i].wasCopyAssigned) || q[i].wasMoveConstructed));
        REQUIRE(q[i].value == 5 + i);
    }
}

TEST_CASE("Queue popBack", "[queue]") {
    RecordType::resetStats();
    Queue<RecordType> q({ 2, 4, 6 });
    RecordType r = q.popBack();
    REQUIRE(r.value == 6);
    REQUIRE(q.size() == 2);
    REQUIRE(q[0].value == 2);
    REQUIRE(q[1].value == 4);
    REQUIRE(RecordType::existingNum() == 3);
    REQUIRE_ASSERT(q[2]);

    r = q.popBack();
    REQUIRE(r.value == 4);
    REQUIRE(q.size() == 1);
    REQUIRE(RecordType::existingNum() == 2);
    r = q.popBack();
    REQUIRE(r.value == 2);
    REQUIRE(q.size() == 0);
    REQUIRE(RecordType::existingNum() == 1);
    REQUIRE_ASSERT(q.popBack());
}

TEST_CASE("Queue popFront", "[queue]") {
    RecordType::resetStats();
    Queue<RecordType> q({ 2, 4, 6 });
    RecordType r = q.popFront();
    REQUIRE(r.value == 2);
    REQUIRE(q.size() == 2);
    REQUIRE(q[0].value == 4);
    REQUIRE(q[1].value == 6);
    REQUIRE(RecordType::existingNum() == 3);
    REQUIRE_ASSERT(q[2]);

    r = q.popFront();
    REQUIRE(r.value == 4);
    REQUIRE(q.size() == 1);
    REQUIRE(RecordType::existingNum() == 2);
    r = q.popFront();
    REQUIRE(r.value == 6);
    REQUIRE(q.size() == 0);
    REQUIRE(RecordType::existingNum() == 1);
    REQUIRE_ASSERT(q.popBack());
}

TEST_CASE("Queue clear", "[queue]") {
    Queue<RecordType> q({ 2, 4, 6, 8 });
    RecordType::resetStats();
    q.clear();
    REQUIRE(q.size() == 0);
    REQUIRE(q.empty());
    REQUIRE(RecordType::destructedNum == 4);

    // check that queue is in consistent state
    q.pushFront(3);
    q.pushBack(7);
    q.pushFront(1);
    REQUIRE(q.size() == 3);
    REQUIRE(q[0].value == 1);
    REQUIRE(q[1].value == 3);
    REQUIRE(q[2].value == 7);
}

TEST_CASE("Queue forward push/pop", "[queue]") {
    Queue<RecordType> q({ 1, 2, 3 });
    for (Size i = 4; i < 1000; ++i) {
        q.pushBack(i);
        q.popFront();
    }
    REQUIRE(q.size() == 3);
    REQUIRE(q[0].value == 997);
    REQUIRE(q[1].value == 998);
    REQUIRE(q[2].value == 999);
}

TEST_CASE("Queue backward push/pop", "[queue]") {
    Queue<RecordType> q({ 3, 2, 1 });
    for (Size i = 4; i < 1000; ++i) {
        q.pushFront(i);
        q.popBack();
    }
    REQUIRE(q.size() == 3);
    REQUIRE(q[0].value == 999);
    REQUIRE(q[1].value == 998);
    REQUIRE(q[2].value == 997);
}

TEST_CASE("Queue forward backward combine", "[queue]") {
    Queue<RecordType> q;
    for (Size i = 0; i < 50; ++i) {
        if (i % 2 == 0) {
            q.pushBack(i);
        } else {
            q.pushFront(i);
        }
    }
    REQUIRE(q.size() == 50);
    REQUIRE(q.front().value == 49);
    REQUIRE(q.back().value == 48);

    for (Size i = 0; i < 25; ++i) {
        if (i % 2 == 0) {
            q.popFront();
        } else {
            q.popBack();
        }
    }
    REQUIRE(q.size() == 25);

    for (Size i = 0; i < 75; ++i) {
        if (i % 5 == 0) {
            q.pushBack(i);
        } else {
            q.pushFront(i);
        }
    }
    REQUIRE(q.size() == 100);
    REQUIRE(q.back().value == 70);
    REQUIRE(q.front().value == 74);
}

TEST_CASE("Queue iterate", "[queue]") {
    // create some non-trivial queue
    Queue<RecordType> q({ 1, 2, 3, 4, 5 });
    for (Size i = 6; i < 1000; ++i) {
        q.pushBack(i);
        q.popFront();
    }
    REQUIRE(q.size() == 5);
    Size i = 0;
    for (RecordType& r : q) {
        REQUIRE(&r == &q[i]);
        REQUIRE(r.value == 995 + i);
        ++i;
    }
    REQUIRE(i == 5);
}
