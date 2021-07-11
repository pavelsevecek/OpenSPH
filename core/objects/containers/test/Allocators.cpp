#include "objects/containers/Allocators.h"
#include "catch.hpp"
#include "objects/containers/List.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Stack allocator", "[allocator]") {
    List<int, StackAllocator<3 * sizeof(ListNode<int>), 1>> list;
    REQUIRE_NOTHROW(list.pushBack(0));
    REQUIRE_NOTHROW(list.pushBack(1));
    REQUIRE_NOTHROW(list.pushBack(2));
    REQUIRE_SPH_ASSERT(list.pushBack(3));
}

TEST_CASE("Stack-mallocator fallback allocator", "[allocator]") {
    using Allocator = FallbackAllocator<              //
        StackAllocator<3 * sizeof(ListNode<int>), 1>, //
        TrackingAllocator<Mallocator>                 //
        >;
    List<int, Allocator> list;
    list.pushBack(0);
    list.pushBack(1);
    list.pushBack(2);
    REQUIRE(list.allocator().fallback().allocated() == 0);
    list.pushBack(3);
    REQUIRE(list.allocator().fallback().allocated() == sizeof(ListNode<int>));
    list.erase(std::next(list.begin(), 3));
    REQUIRE(list.allocator().fallback().allocated() == 0);
}

TEST_CASE("Memory resource allocator", "[allocator]") {
    using Resource = MonotonicMemoryResource<Mallocator>;
    Resource resource(1 << 15, 1);
    List<int, MemoryResourceAllocator<Resource>> list;
    REQUIRE_SPH_ASSERT(list.pushBack(0));

    list.allocator().bind(resource);
    REQUIRE_NOTHROW(list.pushBack(0));
    REQUIRE_NOTHROW(list.erase(list.begin()));
}
