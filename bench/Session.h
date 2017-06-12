#pragma once

#include "common/ForwardDecl.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/Outcome.h"
#include "objects/wrappers/Range.h"
#include "objects/wrappers/SharedPtr.h"
#include "system/Timer.h"

#define NAMESPACE_BENCHMARK_BEGIN NAMESPACE_SPH_BEGIN namespace Benchmark {
#define NAMESPACE_BENCHMARK_END                                                                              \
    }                                                                                                        \
    NAMESPACE_SPH_END

NAMESPACE_BENCHMARK_BEGIN

class Stats {
private:
    Float sum = 0._f;
    Float sumSqr = 0._f;
    Size cnt = 0;
    Range range;

public:
    INLINE void add(const Float value) {
        sum += value;
        sumSqr += sqr(value);
        range.extend(value);
        cnt++;
    }

    INLINE Float mean() const {
        ASSERT(cnt != 0);
        return sum / cnt;
    }

    INLINE Float variance() const {
        if (cnt < 2) {
            return INFTY;
        }
        const Float cntInv = 1._f / cnt;
        return cntInv * (sumSqr * cntInv - sqr(sum * cntInv));
    }

    INLINE Size count() const {
        return cnt;
    }

    INLINE Float min() const {
        return range.lower();
    }

    INLINE Float max() const {
        return range.upper();
    }
};

/// Specifies the ending condition of the benchmark
struct LimitCondition {
    enum class Type {
        VARIANCE,  ///< Mean variance of iteration gets below the threshold
        TIME,      ///< Benchmark ran for given duration
        ITERATION, ///< Benchmark performed given number of iteration
    };

    Type type = Type::ITERATION;
    Float variance = 0.05_f; // 5%
    Size duration = 500;     // 500ms
    Size iterationCnt = 5;
};


class Context {
private:
    bool state = true;

    Size iterationCnt = 0;

    LimitCondition limit;

    Stats& stats;

    Timer iterationTimer;

    Timer totalTimer;

    /// Name of the running benchmark
    std::string& name;

public:
    Context(const LimitCondition limit, Stats& stats, std::string& name)
        : limit(limit)
        , stats(stats)
        , name(name) {}

    /// Whether to keep running or exit
    INLINE bool running() {
        state = this->shouldContinue();
        if (state) {
            iterationCnt++;
            stats.add(iterationTimer.elapsed(TimerUnit::MILLISECOND));
            iterationTimer.restart();
        }
        return state;
    }

    INLINE Size elapsed() const {
        return totalTimer.elapsed(TimerUnit::MILLISECOND);
    }

private:
    INLINE bool shouldContinue() const {
        switch (limit.type) {
        case LimitCondition::Type::ITERATION:
            return stats.count() < limit.iterationCnt;
        case LimitCondition::Type::TIME:
            return totalTimer.elapsed(TimerUnit::MILLISECOND) < limit.duration;
        case LimitCondition::Type::VARIANCE:
            return stats.variance() > sqr(limit.variance);
        default:
            NOT_IMPLEMENTED;
        }
    }
};

/// Single benchmark unit
class Unit {
private:
    std::string name;

    std::function<void(Context&)> func;

public:
    Unit(const std::string& name, std::function<void(Context&)> func)
        : name(name)
        , func(std::move(func)) {
        ASSERT(this->func != nullptr);
    }

    const std::string& getName() const {
        return name;
    }

    Outcome run(Stats& stats, Size& elapsed) {
        Context context(LimitCondition(), stats, name);
        func(context);
        elapsed = context.elapsed();
        return SUCCESS;
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
    AutoPtr<Abstract::Logger> logger;

    /// Status of the session, contains an error if the session is in invalid state.
    Outcome status = SUCCESS;

    enum class Flag {
        COMPARE_WITH_BASELINE = 1 << 0, ///< Compare results with baseline

        MAKE_BASELINE = 1 << 1, ///< Record and cache baseline

        SILENT = 1 << 2, ///< Only print failed benchmarks
    };

    struct {
        /// Run only selected group of benchmarks
        std::string group;

        Flags<Flag> flags;
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

    void log(const std::string& text);

    void logError(const std::string& text);
};

class BaselineWriter {
private:
    // baseline on number of iterations -> test run also on iterations, measure time
    // baseline on varince -> test on variance, measure time and iterations?
    // baseline on time -> test on time, measure iteartiosn
};

/// \todo param, warning for too fast/too slow units
/// \todo add comparing benchmarks, running two functions and comparing, instead of comparing against baseline

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
