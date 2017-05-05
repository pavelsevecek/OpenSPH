#pragma once

/// \file Profiler.h
/// \brief Tool to measure time spent in functions and profile the code
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "system/Timer.h"
#include <atomic>
#include <map>


NAMESPACE_SPH_BEGIN


/// Timer that reports the measured duration when being destroyed. If measured scope is executed by multiple
/// thread at once, the total time is equal to the sum of all per-thread times.
struct ScopedTimer {
private:
    StoppableTimer impl;
    std::string name;

    using OnScopeEnds = std::function<void(const std::string&, const uint64_t)>;
    OnScopeEnds callback;

public:
    ScopedTimer(const std::string& name, const OnScopeEnds& callback)
        : name(name)
        , callback(callback) {}

    ~ScopedTimer() {
        callback(name, impl.elapsed(TimerUnit::MICROSECOND));
    }

    void stop() {
        impl.stop();
    }

    void resume() {
        impl.resume();
    }

    void next(const std::string& newName) {
        callback(name, impl.elapsed(TimerUnit::MICROSECOND));
        impl.restart();
        name = newName;
    }
};

#define MEASURE_SCOPE(name)
/*ScopedTimer __timer("", [](const std::string&, const uint64_t time) {
    std::cout << name << " took " << time / 1000 << " ms" << std::endl;
});*/


struct ScopeStatistics {
    std::string name;
    uint64_t totalTime; // time spent in function (in ms)
    float relativeTime;
};

namespace Abstract {
    class Logger;
}

/// Profiler object implemented as singleton.
class Profiler : public Noncopyable {
private:
    static AutoPtr<Profiler> instance;

    struct Duration {
        std::atomic<uint64_t> time;

        Duration() {
            time = 0; // initialize to zero
        }
    };
    // map of profiled scopes, its key being a string = name of the scope
    std::map<std::string, Duration> map;


public:
    static Profiler* getInstance() {
        if (!instance) {
            instance = makeAuto<Profiler>();
        }
        return instance.get();
    }

    /// Creates a new scoped timer of given name. The timer will automatically adds elapsed time to the
    /// profile when being destroyed.
    ScopedTimer makeScopedTimer(const std::string& name) {
        return ScopedTimer(name, [this](const std::string& n, const uint64_t elapsed) { //
            map[n].time += elapsed;
        });
    }

    /// Returns the array of scope statistics, sorted by elapsed time.
    Array<ScopeStatistics> getStatistics() const;

    /// Prints statistics into the logger.
    void printStatistics(Abstract::Logger& logger) const;

    /// Clears all records, mainly for testing purposes
    void clear() {
        map.clear();
    }
};

#ifdef SPH_PROFILE
#define PROFILE_SCOPE(name)                                                                                  \
    Profiler* __instance = Profiler::getInstance();                                                          \
    ScopedTimer __scopedTimer = __instance->makeScopedTimer(name);
//#define PROFILE_NEXT(name) __scopedTimer.next(name);
//#define SCOPE_STOP __scopedTimer.stop()
//#define SCOPE_RESUME __scopedTimer.resume()
#else
#define PROFILE_SCOPE(name)
#define SCOPE_STOP
#define SCOPE_RESUME
#define PROFILE_NEXT(name)
#endif

NAMESPACE_SPH_END
