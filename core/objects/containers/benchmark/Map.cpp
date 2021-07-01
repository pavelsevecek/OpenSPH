#include "bench/Session.h"
#include "objects/containers/FlatMap.h"
#include <map>


using namespace Sph;

template <typename TMap>
static void benchmarkMap(TMap& map, Benchmark::Context& context) {
    std::hash<int> hash;
    constexpr int num = 10000;
    while (context.running()) {
        Size sum = 0;
        for (Size i = 0; i < num; ++i) {
            Benchmark::doNotOptimize(sum += map[hash(i) % map.size()]);
        }
        Benchmark::clobberMemory();
    }
}

BENCHMARK("std::map 10", "[flatmap]", Benchmark::Context& context) {
    std::map<int, std::size_t> map;
    for (Size i = 0; i < 10; ++i) {
        map.insert({ i, std::hash<int>{}(i) });
    }
    benchmarkMap(map, context);
}

BENCHMARK("Sph::Map 10", "[flatmap]", Benchmark::Context& context) {
    FlatMap<int, std::size_t> map;
    for (Size i = 0; i < 10; ++i) {
        map.insert(i, std::hash<int>{}(i));
    }
    benchmarkMap(map, context);
}


BENCHMARK("std::map 100", "[flatmap]", Benchmark::Context& context) {
    std::map<int, std::size_t> map;
    for (Size i = 0; i < 100; ++i) {
        map.insert({ i, std::hash<int>{}(i) });
    }
    benchmarkMap(map, context);
}

BENCHMARK("Sph::Map 100", "[flatmap]", Benchmark::Context& context) {
    FlatMap<int, std::size_t> map;
    for (Size i = 0; i < 100; ++i) {
        map.insert(i, std::hash<int>{}(i));
    }
    benchmarkMap(map, context);
}

BENCHMARK("std::map 1000", "[flatmap]", Benchmark::Context& context) {
    std::map<int, std::size_t> map;
    for (Size i = 0; i < 1000; ++i) {
        map.insert({ i, std::hash<int>{}(i) });
    }
    benchmarkMap(map, context);
}

BENCHMARK("Sph::Map 1000", "[flatmap]", Benchmark::Context& context) {
    FlatMap<int, std::size_t> map;
    for (Size i = 0; i < 1000; ++i) {
        map.insert(i, std::hash<int>{}(i));
    }
    benchmarkMap(map, context);
}

BENCHMARK("std::map 10000", "[flatmap]", Benchmark::Context& context) {
    std::map<int, std::size_t> map;
    for (Size i = 0; i < 10000; ++i) {
        map.insert({ i, std::hash<int>{}(i) });
    }
    benchmarkMap(map, context);
}

BENCHMARK("Sph::Map 10000", "[flatmap]", Benchmark::Context& context) {
    FlatMap<int, std::size_t> map;
    for (Size i = 0; i < 10000; ++i) {
        map.insert(i, std::hash<int>{}(i));
    }
    benchmarkMap(map, context);
}
