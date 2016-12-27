#include "system/CallbackList.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("CallbackList", "[callbacklist]") {
    CallbackList<void(int, float)> list;

    struct Parent : public Observable {
        int i;
        float f;
    };
    Parent p1;
    list.add(p1, [&p1](int i, float f) {
        p1.i = i;
        p1.f = f;
    });

    Parent p2;
    list.add(p2, [&p2](int i, float f) {
        p2.i = i + 2;
        p2.f = f + 1.5f;
    });

    {
        Parent p3;
        list.add(p3, [](int, float) {});
        // expired parent
    }
    list(6, 2.f);
    REQUIRE(p1.i == 6);
    REQUIRE(p1.f == 2.f);
    REQUIRE(p2.i == 8);
    REQUIRE(p2.f == 3.5f);
}
