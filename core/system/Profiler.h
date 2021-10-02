#pragma once

/// \file Profiler.h
/// \brief Tool to measure time spent in functions and profile the code
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

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
    String name;

    using OnScopeEnds = std::function<void(const String&, const uint64_t)>;
    OnScopeEnds callback;

public:
    /// Creates a scoped time.
    /// \param name User-defined name of the timer.
    /// \param callback Function called when the timer goes out of scoped. The timer passes its name and
    ///                 elapsed time as parameters of the function.
    ScopedTimer(const String& name, const OnScopeEnds& callback)
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

    void next(const String& newName) {
        callback(name, impl.elapsed(TimerUnit::MICROSECOND));
        impl.restart();
        name = newName;
    }
};

#ifdef SPH_PROFILE
#define MEASURE_SCOPE(name)                                                                                  \
    ScopedTimer __timer("", [](const String&, const uint64_t time) {                                         \
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
    String name;

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
    std::map<String, ScopeRecord> records;

    struct {
        String currentScope;
        std::thread thread;
    } cpuUsage;

    std::atomic_bool quitting{ false };

public:
    Profiler();

    ~Profiler();

    static Profiler& getInstance() {
        if (!instance) {
            instance = makeAuto<Profiler>();
        }
        return *instance;
    }

    /// \brief Creates a new scoped timer of given name.
    ///
    /// The timer will automatically adds elapsed time to the profile when being destroyed.
    ScopedTimer makeScopedTimer(const String& name) {
        String __previousScope = cpuUsage.currentScope;
        cpuUsage.currentScope = name;
        return ScopedTimer(name, [=](const String& n, const uint64_t elapsed) { //
            records[n].duration += elapsed;
            cpuUsage.currentScope = __previousScope;
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
