#include "objects/containers/Map.h"
#include "bench/Session.h"
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

BENCHMARK("std::map 10", "[map]", Benchmark::Context& context) {
    std::map<int, std::size_t> map;
    for (Size i = 0; i < 10; ++i) {
        map.insert({ i, std::hash<int>{}(i) });
    }
    benchmarkMap(map, context);
}

BENCHMARK("Sph::Map 10", "[map]", Benchmark::Context& context) {
    Map<int, std::size_t> map;
    for (Size i = 0; i < 10; ++i) {
        map.insert(i, std::hash<int>{}(i));
    }
    benchmarkMap(map, context);
}

BENCHMARK("Sph::SmallMap 10", "[map]", Benchmark::Context& context) {
    Map<int, std::size_t, MapOptimization::SMALL> map;
    for (Size i = 0; i < 10; ++i) {
        map.insert(i, std::hash<int>{}(i));
    }
    benchmarkMap(map, context);
}

BENCHMARK("std::map 100", "[map]", Benchmark::Context& context) {
    std::map<int, std::size_t> map;
    for (Size i = 0; i < 100; ++i) {
        map.insert({ i, std::hash<int>{}(i) });
    }
    benchmarkMap(map, context);
}

BENCHMARK("Sph::Map 100", "[map]", Benchmark::Context& context) {
    Map<int, std::size_t> map;
    for (Size i = 0; i < 100; ++i) {
        map.insert(i, std::hash<int>{}(i));
    }
    benchmarkMap(map, context);
}

BENCHMARK("Sph::SmallMap 100", "[map]", Benchmark::Context& context) {
    Map<int, std::size_t, MapOptimization::SMALL> map;
    for (Size i = 0; i < 100; ++i) {
        map.insert(i, std::hash<int>{}(i));
    }
    benchmarkMap(map, context);
}

BENCHMARK("std::map 1000", "[map]", Benchmark::Context& context) {
    std::map<int, std::size_t> map;
    for (Size i = 0; i < 1000; ++i) {
        map.insert({ i, std::hash<int>{}(i) });
    }
    benchmarkMap(map, context);
}

BENCHMARK("Sph::Map 1000", "[map]", Benchmark::Context& context) {
    Map<int, std::size_t> map;
    for (Size i = 0; i < 1000; ++i) {
        map.insert(i, std::hash<int>{}(i));
    }
    benchmarkMap(map, context);
}

BENCHMARK("Sph::SmallMap 1000", "[map]", Benchmark::Context& context) {
    Map<int, std::size_t, MapOptimization::SMALL> map;
    for (Size i = 0; i < 1000; ++i) {
        map.insert(i, std::hash<int>{}(i));
    }
    benchmarkMap(map, context);
}

BENCHMARK("std::map 10000", "[map]", Benchmark::Context& context) {
    std::map<int, std::size_t> map;
    for (Size i = 0; i < 10000; ++i) {
        map.insert({ i, std::hash<int>{}(i) });
    }
    benchmarkMap(map, context);
}

BENCHMARK("Sph::Map 10000", "[map]", Benchmark::Context& context) {
    Map<int, std::size_t> map;
    for (Size i = 0; i < 10000; ++i) {
        map.insert(i, std::hash<int>{}(i));
    }
    benchmarkMap(map, context);
}
