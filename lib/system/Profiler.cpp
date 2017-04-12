#include "system/Profiler.h"
#include "io/Logger.h"
#include <algorithm>
#include <iomanip>

NAMESPACE_SPH_BEGIN

std::unique_ptr<Profiler> Profiler::instance;

Array<ScopeStatistics> Profiler::getStatistics() const {
    Array<ScopeStatistics> stats;
    uint64_t totalTime = 0;
    for (auto& iter : map) {
        stats.push(ScopeStatistics{ iter.first, iter.second.time, 0 });
        totalTime += iter.second.time;
    }
    for (auto& s : stats) {
        s.relativeTime = float(s.totalTime) / float(totalTime);
    }
    std::sort(stats.begin(), stats.end(), [](ScopeStatistics& s1, ScopeStatistics& s2) {
        return s1.totalTime > s2.totalTime;
    });
    return stats;
}

void Profiler::printStatistics(Abstract::Logger& logger) const {
    Array<ScopeStatistics> stats = this->getStatistics();
    for (ScopeStatistics& s : stats) {
        std::stringstream ss;
        ss << std::setw(55) << std::left << s.name << " | " << std::setw(10) << std::right << s.totalTime
           << "mus   "
           << "|" << std::setw(10) << std::right << std::setprecision(3) << std::fixed
           << 100._f * s.relativeTime << "%";
        logger.write(ss.str());
    }
}

NAMESPACE_SPH_END
