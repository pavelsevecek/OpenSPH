#include "catch.hpp"
#include "storage/BasicView.h"

using namespace Sph;

TEST_CASE("Iterate", "[iterables]") {
    GenericStorage storage;
    std::unique_ptr<BasicView> view(storage.emplace<BasicView>());
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

    iterate<IterableType::ALL>(storage, [](auto&& v) {
        using TArray = std::decay_t<decltype(v)>;
        using T = std::decay_t<decltype(std::declval<TArray>()[std::declval<int>()])>;
        v.push(T(10._f));
    });
    REQUIRE(view->rs[5] == Vector(10._f));
    REQUIRE(view->rhos[3] == 10._f);
}
