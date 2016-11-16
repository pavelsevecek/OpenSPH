#pragma once

#include "system/Timer.h"
#include <memory>
#include <map>

NAMESPACE_SPH_BEGIN


/// Timer that reports the measured duration when being destroyed
struct ScopedTimer : public Object {
private:
    Timer impl;
    std::string name;

    using OnScopeEnds = std::function<void(const std::string&, const uint64_t)>;
    OnScopeEnds callback;

public:
    ScopedTimer(const std::string& name, const OnScopeEnds& callback)
        : name(name)
        , callback(callback) {}

    ~ScopedTimer() { callback(name, impl.elapsed()); }
};

struct ScopeStatistics {
    std::string name;
    uint64_t totalTime; // time spent in function (in ms)
    float relativeTime;
};

/// Profiler object implemented as singleton.
class Profiler : public Noncopyable {
private:
    static std::unique_ptr<Profiler> instance;

    struct Duration {
        uint64_t time = 0; // initialize to zerp
    };
    // map of profiled scopes, its key being a string = name of the scope
    std::map<std::string, Duration> map;

public:
    static Profiler* getInstance() {
        if (!instance) {
            instance = std::make_unique<Profiler>();
        }
        return instance.get();
    }

    ScopedTimer makeScopedTimer(const std::string& name) {
        return ScopedTimer(name, [this](const std::string& n, const uint64_t elapsed) {
            ASSERT(elapsed > 0 && "too small scope to be measured in ms, make timer in microseconds");
            map[n].time += elapsed;
        });
    }

    Array<ScopeStatistics> getStatistics() const;
};

#ifdef PROFILE
#define PROFILE_SCOPE(name)                                                                                    \
    Profiler* __instance      = Profiler::getInstance();                                                     \
    ScopedTimer __scopedTimer = __instance->makeScopedTimer(name);
#else
#define PROFILE_SCOPE(name)
#endif

NAMESPACE_SPH_END
