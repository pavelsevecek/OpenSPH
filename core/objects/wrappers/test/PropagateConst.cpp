#include "objects/wrappers/PropagateConst.h"
#include "catch.hpp"
#include "objects/wrappers/AutoPtr.h"
#include "utils/Utils.h"

using namespace Sph;

struct ConstChecker {
    int nonConstCalled = 0;
    mutable int constCalled = 0;

    void call() {
        nonConstCalled++;
    }
    void call() const {
        constCalled++;
    }
};

TEST_CASE("AutoPtr const behavior", "[propagateconst]") {
    AutoPtr<ConstChecker> ptr = makeAuto<ConstChecker>();
    ptr->call();
    REQUIRE(ptr->nonConstCalled == 1);
    REQUIRE(ptr->constCalled == 0);
    asConst(ptr)->call();
    REQUIRE(ptr->nonConstCalled == 2);
    REQUIRE(ptr->constCalled == 0);
    (*ptr).call();
    REQUIRE(ptr->nonConstCalled == 3);
    REQUIRE(ptr->constCalled == 0);
    (*asConst(ptr)).call();
    REQUIRE(ptr->nonConstCalled == 4);
    REQUIRE(ptr->constCalled == 0);
}

TEST_CASE("PropagateConst const behavior", "[propagateconst]") {
    PropagateConst<AutoPtr<ConstChecker>> ptr = makeAuto<ConstChecker>();
    ptr->call();
    REQUIRE(ptr->nonConstCalled == 1);
    REQUIRE(ptr->constCalled == 0);
    asConst(ptr)->call();
    REQUIRE(ptr->nonConstCalled == 1);
    REQUIRE(ptr->constCalled == 1);
    (*ptr).call();
    REQUIRE(ptr->nonConstCalled == 2);
    REQUIRE(ptr->constCalled == 1);
    (*asConst(ptr)).call();
    REQUIRE(ptr->nonConstCalled == 2);
    REQUIRE(ptr->constCalled == 2);
}
