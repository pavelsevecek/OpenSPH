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
#include "objects/utility/StringUtils.h"
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
    std::string name;

public:
    Context(const Target target, const std::string& name)
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
    std::string name;

    using Function = void (*)(Context&);
    Function function;

public:
    Unit(const std::string& name, const Function func)
        : name(name)
        , function(func) {
        SPH_ASSERT(function != nullptr);
    }

    const std::string& getName() const {
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
    std::string name;
    Array<SharedPtr<Unit>> benchmarks;

public:
    Group() = default;

    Group(const std::string& name)
        : name(name) {}

    const std::string& getName() const {
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
    std::map<std::string, Result> benchs;

public:
    Baseline() = default;

    bool parse(const Path& path) {
        benchs.clear();
        std::ifstream ifs(path.native());
        std::string line;
        while (std::getline(ifs, line)) {
            std::size_t n = line.find_last_of('/');
            if (n == std::string::npos) {
                return false;
            }
            const std::string name = trim(line.substr(0, n));
            Array<std::string> values = split(line.substr(n + 1), ',');
            if (values.size() != 6) {
                return false;
            }
            Result result;
            result.duration = std::stoi(values[0]);
            result.iterateCnt = std::stoi(values[1]);
            result.mean = std::stof(values[2]);
            result.variance = std::stof(values[3]);
            result.min = std::stof(values[4]);
            result.max = std::stof(values[5]);

            benchs[name] = result;
        }
        return true;
    }

    bool isRecorded(const std::string& name) {
        return benchs.find(name) != benchs.end();
    }

    INLINE Result operator[](const std::string& name) {
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
        std::string group;

        Flags<Flag> flags;

        struct {
            Path path;
            Size commit = 0;
        } baseline;

        Array<std::string> benchmarksToRun;

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
    void registerBenchmark(const SharedPtr<Unit>& benchmark, const std::string& groupName);

    /// Runs all benchmarks.
    void run(int argc, char* argv[]);

    ~Session();

private:
    Group& getGroupByName(const std::string& groupName);

    Outcome parseArgs(int arcs, char* argv[]);

    void printHelp();

    void writeBaseline(const std::string& name, const Result& measured);

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
    Register(const SharedPtr<Unit>& benchmark, const std::string& groupName);
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
