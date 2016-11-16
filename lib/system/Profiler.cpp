#include "system/Profiler.h"
#include <algorithm>

NAMESPACE_SPH_BEGIN

std::unique_ptr<Profiler> Profiler::instance;

Array<ScopeStatistics> Profiler::getStatistics() const {
    Array<ScopeStatistics> stats;
    uint64_t totalTime = 0;
    for (auto& iter : map) {
        stats.push(ScopeStatistics{iter.first, iter.second.time, 0});
        totalTime += iter.second.time;
    }
    for (auto& s : stats) {
        s.relativeTime = float(s.totalTime) / float(totalTime);
    }
    std::sort(stats.begin(), stats.end(), [](ScopeStatistics& s1, ScopeStatistics& s2){
        return s1.totalTime > s2.totalTime;
    });
    return stats;
}

NAMESPACE_SPH_END
