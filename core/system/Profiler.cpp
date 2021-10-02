#include "system/Profiler.h"
#include "io/Logger.h"
#include <algorithm>

NAMESPACE_SPH_BEGIN

#ifdef SPH_PROFILE
AutoPtr<Profiler> Profiler::instance;

Profiler::Profiler() {
    cpuUsage.thread = std::thread([this] {
        while (!quitting) {
            const Optional<Float> usage = getCpuUsage();
            if (usage && !cpuUsage.currentScope.empty()) {
                ScopeRecord& scope = records[cpuUsage.currentScope];
                scope.cpuUsage = (scope.cpuUsage * scope.weight + usage.value()) / (scope.weight + 1);
                scope.weight++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
}

Profiler::~Profiler() {
    quitting = true;
    cpuUsage.thread.join();
}

Array<ScopeStatistics> Profiler::getStatistics() const {
    Array<ScopeStatistics> stats;
    uint64_t totalTime = 0;
    for (auto& iter : records) {
        stats.push(ScopeStatistics{ iter.first, iter.second.duration, 0, iter.second.cpuUsage });
        totalTime += iter.second.duration;
    }
    for (auto& s : stats) {
        s.relativeTime = float(s.totalTime) / float(totalTime);
    }
    std::sort(stats.begin(), stats.end(), [](ScopeStatistics& s1, ScopeStatistics& s2) {
        return s1.totalTime > s2.totalTime;
    });
    return stats;
}

void Profiler::printStatistics(ILogger& logger) const {
    Array<ScopeStatistics> stats = this->getStatistics();
    for (ScopeStatistics& s : stats) {
        std::stringstream ss;
        ss << std::setw(45) << std::left << s.name << " | " << std::setw(10) << std::right << s.totalTime
           << "mus   | rel: " << std::setw(8) << std::right << std::setprecision(3) << std::fixed
           << 100._f * s.relativeTime << "%  | cpu: " << std::setw(8) << std::right << std::setprecision(3)
           << std::fixed << 100._f * s.cpuUsage << "%";
        logger.write(String::fromAscii(ss.str().c_str()));
    }
}
#endif

NAMESPACE_SPH_END
