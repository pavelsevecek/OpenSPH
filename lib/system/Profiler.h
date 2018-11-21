#pragma once

/// \file Profiler.h
/// \brief Tool to measure time spent in functions and profile the code
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "io/Logger.h"
#include "objects/wrappers/Optional.h"
#include "system/Platform.h"
#include "system/Timer.h"
#include <atomic>
#include <map>
#include <thread>

NAMESPACE_SPH_BEGIN

/// \brief Timer that reports the measured duration when being destroyed.
///
/// If measured scope is executed by multiple thread at once, the total time is equal to the sum of all
/// per-thread times.
struct ScopedTimer {
private:
    StoppableTimer impl;
    std::string name;

    using OnScopeEnds = std::function<void(const std::string&, const uint64_t)>;
    OnScopeEnds callback;

public:
    /// Creates a scoped time.
    /// \param name User-defined name of the timer.
    /// \param callback Function called when the timer goes out of scoped. The timer passes its name and
    ///                 elapsed time as parameters of the function.
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

#ifdef SPH_PROFILE
#define MEASURE_SCOPE(name)                                                                                  \
    ScopedTimer __timer("", [](const std::string&, const uint64_t time) {                                    \
        StdOutLogger logger;                                                                                 \
        logger.write(name, " took ", time / 1000, " ms");                                                    \
    });
#define MEASURE(name, what)                                                                                  \
    {                                                                                                        \
        MEASURE_SCOPE(name);                                                                                 \
        what;                                                                                                \
    }
#else
#define MEASURE_SCOPE(name)
#define MEASURE(name, what) what
#endif

#ifdef SPH_PROFILE

struct ScopeStatistics {
    /// User defined name of the scope
    std::string name;

    /// Time spent in the scope (in ms)
    uint64_t totalTime;

    /// Relative time spent in the scope with a respect to all measured scopes. Relative times of all measured
    /// scoped sum up to 1.
    Float relativeTime;

    Float cpuUsage;
};


/// Profiler object implemented as singleton.
class Profiler : public Noncopyable {
private:
    static AutoPtr<Profiler> instance;

    struct ScopeRecord {
        /// Total time spent inside the scope
        std::atomic<uint64_t> duration{ 0 };

        /// Average cpu usage inside the scope
        Float cpuUsage = 0._f;

        /// Number of samples used to compute the cpu Usage
        Size weight = 0;
    };

    // map of profiled scopes, its key being a string = name of the scope
    std::map<std::string, ScopeRecord> records;

    struct {
        std::string currentScope;
        std::thread thread;
    } cpuUsage;

    std::atomic_bool quitting{ false };

public:
    Profiler() {
        cpuUsage.thread = std::thread([this] {
            while (!quitting) {
                const Optional<Float> usage = getCpuUsage();
                if (usage && !cpuUsage.currentScope.empty()) {
                    ScopeRecord& scope = records[cpuUsage.currentScope];
                    scope.cpuUsage = (scope.cpuUsage * scope.weight + usage.value()) / (scope.weight + 1);
                    scope.weight++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        });
    }

    ~Profiler() {
        quitting = true;
        cpuUsage.thread.join();
    }

    static Profiler& getInstance() {
        if (!instance) {
            instance = makeAuto<Profiler>();
        }
        return *instance;
    }

    /// \brief Creates a new scoped timer of given name.
    ///
    /// The timer will automatically adds elapsed time to the profile when being destroyed.
    ScopedTimer makeScopedTimer(const std::string& name) {
        cpuUsage.currentScope = name;
        return ScopedTimer(name, [this](const std::string& n, const uint64_t elapsed) { //
            records[n].duration += elapsed;
        });
    }

    /// Returns the array of scope statistics, sorted by elapsed time.
    Array<ScopeStatistics> getStatistics() const;

    /// Prints statistics into the logger.
    void printStatistics(ILogger& logger) const;

    /// Clears all records, mainly for testing purposes
    void clear() {
        records.clear();
    }
};

#define PROFILE_SCOPE(name)                                                                                  \
    Profiler& __instance = Profiler::getInstance();                                                          \
    ScopedTimer __scopedTimer = __instance.makeScopedTimer(name);
#define PROFILE(name, what)                                                                                  \
    {                                                                                                        \
        PROFILE_SCOPE(name);                                                                                 \
        what;                                                                                                \
    }
#else
#define PROFILE_SCOPE(name)
#define PROFILE(name, what) what
#endif

NAMESPACE_SPH_END
