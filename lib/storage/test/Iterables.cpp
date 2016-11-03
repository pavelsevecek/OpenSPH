#include "catch.hpp"
#include "storage/BasicView.h"

using namespace Sph;

TEST_CASE("Iterate", "[iterables]") {
    GenericStorage storage;
    std::unique_ptr<BasicView> view(storage.makeViewer<BasicView>());
    iterate<IterableType::SECOND_ORDER>(storage, [](auto&& v, auto&& dv, auto&& d2v) {
        v.resize(5);
        dv.resize(5);
        d2v.resize(5);
    });

    iterate<IterableType::FIRST_ORDER>(storage, [](auto&& v, auto&& dv) {
        v.resize(3);
        dv.resize(3);
    });

    REQUIRE(view->rs.size() == 5);
    REQUIRE(view->vs.size() == 5);
    REQUIRE(view->dvs.size() == 5);
    REQUIRE(view->ms.size() == 0);
    REQUIRE(view->rhos.size() == 3);
    REQUIRE(view->drhos.size() == 3);
    REQUIRE(view->ps.size() == 0);
    REQUIRE(view->us.size() == 3);
    REQUIRE(view->dus.size() == 3);
}
