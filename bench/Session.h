#pragma once

/// \file Session.h
/// \brief Benchmark
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "bench/Common.h"
#include "bench/Stats.h"
#include "io/Logger.h"
#include "io/Path.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/Expected.h"
#include "objects/wrappers/Outcome.h"
#include "objects/wrappers/SharedPtr.h"
#include "system/Timer.h"
#include <fstream>
#include <map>

NAMESPACE_BENCHMARK_BEGIN

/// Benchmark mode
enum class Mode {
    SIMPLE,               ///< Just run benchmarks and report statistics
    MAKE_BASELINE,        ///< Store iteration numbers to baseline file
    RUN_AGAINST_BASELINE, ///< Compare iteration numbers with recorded baseline
};

struct Target {
    Mode mode;
    uint64_t duration;
    Size iterateCnt;
};

struct Result {
    uint64_t duration;
    Size iterateCnt;
    Float mean;
    Float variance;
    Float min;
    Float max;
};

/// Accessible from benchmarks
class Context {
private:
    Target target;

    bool state = true;

    Size iterateCnt = 0;

    Timer timer;

    Timer iterationTimer;

    Stats stats;

    /// Name of the running benchmark
    String name;

public:
    Context(const Target target, const String& name)
        : target(target)
        , timer(target.duration)
        , name(name) {}

    /// Whether to keep running or exit
    INLINE bool running() {
        state = this->shouldContinue();
        if (iterateCnt <= 2) {
            // restart to discard benchmark setup time and first few iterations (startup)
            timer.restart();
        } else {
            stats.add(1.e-3_f * iterationTimer.elapsed(TimerUnit::MICROSECOND));
        }
        iterationTimer.restart();
        iterateCnt++;
        return state;
    }

    INLINE uint64_t elapsed() const {
        return timer.elapsed(TimerUnit::MILLISECOND);
    }

    INLINE Size iterationCnt() const {
        return iterateCnt;
    }

    INLINE Stats getStats() const {
        return stats;
    }

    /// Writes given message into the logger
    template <typename... TArgs>
    INLINE void log(TArgs&&... args) {
        StdOutLogger logger;
        logger.write(std::forward<TArgs>(args)...);
    }

private:
    INLINE bool shouldContinue() const {
        switch (target.mode) {
        case Mode::SIMPLE:
        case Mode::MAKE_BASELINE:
            // either not enough time passed, or not enough iterations
            return iterateCnt < target.iterateCnt || !timer.isExpired();
        case Mode::RUN_AGAINST_BASELINE:
            return iterateCnt < target.iterateCnt;
        default:
            NOT_IMPLEMENTED;
        }
    }
};

/// Single benchmark unit
class Unit {
private:
    String name;

    using Function = void (*)(Context&);
    Function function;

public:
    Unit(const String& name, const Function func)
        : name(name)
        , function(func) {
        SPH_ASSERT(function != nullptr);
    }

    const String& getName() const {
        return name;
    }

    Expected<Result> run(const Target target) {
        Context context(target, name);
        function(context);
        uint64_t elapsed = context.elapsed();
        Stats stats = context.getStats();
        return Result{
            elapsed, context.iterationCnt() - 1, stats.mean(), stats.variance(), stats.min(), stats.max()
        };
    }
};

class Group {
private:
    String name;
    Array<SharedPtr<Unit>> benchmarks;

public:
    Group() = default;

    Group(const String& name)
        : name(name) {}

    const String& getName() const {
        return name;
    }

    void addBenchmark(const SharedPtr<Unit>& benchmark) {
        benchmarks.push(benchmark);
    }

    auto begin() const {
        return benchmarks.begin();
    }

    auto end() const {
        return benchmarks.end();
    }

    Size size() const {
        return benchmarks.size();
    }
};

template <class T>
INLINE T&& doNotOptimize(T&& value) {
#if defined(__clang__)
    asm volatile("" : : "g"(value) : "memory");
#else
    asm volatile("" : : "i,r,m"(value) : "memory");
#endif
    return std::forward<T>(value);
}

// Force the compiler to flush pending writes to global memory. Acts as an effective read/write barrier
INLINE void clobberMemory() {
    asm volatile("" : : : "memory");
}

class Baseline {
private:
    std::map<String, Result> benchs;

public:
    Baseline() = default;

    bool parse(const Path& path) {
        benchs.clear();
        std::wifstream ifs(path.native());
        std::wstring wstr;
        while (std::getline(ifs, wstr)) {
            String line = String::fromWstring(wstr);
            std::size_t n = line.findLast(L'/');
            if (n == String::npos) {
                return false;
            }
            const String name = line.substr(0, n).trim();
            Array<String> values = split(line.substr(n + 1), ',');
            if (values.size() != 6) {
                return false;
            }
            Result result;
            result.duration = fromString<int>(values[0]).value();
            result.iterateCnt = fromString<int>(values[1]).value();
            result.mean = fromString<float>(values[2]).value();
            result.variance = fromString<float>(values[3]).value();
            result.min = fromString<float>(values[4]).value();
            result.max = fromString<float>(values[5]).value();

            benchs[name] = result;
        }
        return true;
    }

    bool isRecorded(const String& name) {
        return benchs.find(name) != benchs.end();
    }

    INLINE Result operator[](const String& name) {
        return benchs[name];
    }
};

class Session {
private:
    /// Global instance of the session
    static AutoPtr<Session> instance;

    /// List of all benchmarks in the session
    Array<SharedPtr<Unit>> benchmarks;

    /// Benchmark groups
    Array<Group> groups;

    /// Logger used to output benchmark results
    AutoPtr<ILogger> logger;

    /// Status of the session, contains an error if the session is in invalid state.
    Outcome status = SUCCESS;

    enum class Flag {
        RUN_AGAINST_BASELINE = 1 << 0, ///< Compare results with baseline

        MAKE_BASELINE = 1 << 1, ///< Record and cache baseline

        SILENT = 1 << 2, ///< Only print failed benchmarks
    };

    struct {
        /// Run only selected group of benchmarks
        String group;

        Flags<Flag> flags;

        struct {
            Path path;
            Size commit = 0;
        } baseline;

        Array<String> benchmarksToRun;

        Target target{ Mode::SIMPLE, 500 /*ms*/, 10 };

        Float confidence = 6._f; // sigma

        /// Maximum allowed duration of single benchmark unit; benchmarks running longer that that will
        /// generate a warning.
        uint64_t maxAllowedDuration = 5000 /*ms*/;
    } params;

public:
    Session();

    static Session& getInstance();

    /// Adds a new benchmark into the session.
    void registerBenchmark(const SharedPtr<Unit>& benchmark, const String& groupName);

    /// Runs all benchmarks.
    void run(int argc, char* argv[]);

    ~Session();

private:
    Group& getGroupByName(const String& groupName);

    Outcome parseArgs(int arcs, char* argv[]);

    void printHelp();

    void writeBaseline(const String& name, const Result& measured);

    Path getBaselinePath();

    void compareResults(const Result& measured, const Result& baseline);

    template <typename... TArgs>
    void log(TArgs&&... args);

    template <typename... TArgs>
    void logError(TArgs&&... args);
};

/// \todo param, warning for too fast/too slow units
/// \todo add comparing benchmarks, running two functions and comparing, instead of comparing against
/// baseline

class Register {
public:
    Register(const SharedPtr<Unit>& benchmark, const String& groupName);
};

#define BENCHMARK_UNIQUE_NAME_IMPL(prefix, line) prefix##line

#define BENCHMARK_UNIQUE_NAME(prefix, line) BENCHMARK_UNIQUE_NAME_IMPL(prefix, line)

#define BENCHMARK_FUNCTION_NAME BENCHMARK_UNIQUE_NAME(BENCHMARK, __LINE__)

#define BENCHMARK_REGISTER_NAME BENCHMARK_UNIQUE_NAME(REGISTER, __LINE__)

#define BENCHMARK(name, group, ...)                                                                          \
    static void BENCHMARK_FUNCTION_NAME(__VA_ARGS__);                                                        \
    namespace {                                                                                              \
        ::Sph::Benchmark::Register BENCHMARK_REGISTER_NAME(                                                  \
            makeShared<::Sph::Benchmark::Unit>(name, &BENCHMARK_FUNCTION_NAME),                              \
            group);                                                                                          \
    }                                                                                                        \
    static void BENCHMARK_FUNCTION_NAME(__VA_ARGS__)

NAMESPACE_BENCHMARK_END
