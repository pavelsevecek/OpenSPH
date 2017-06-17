#include "bench/Session.h"
#include "io/FileSystem.h"
#include "io/Logger.h"
#include "system/Platform.h"

NAMESPACE_BENCHMARK_BEGIN

Session::Session() {
    logger = makeAuto<StdOutLogger>();
    logger->setPrecision(3);
    logger->setScientific(false);
}

Session::~Session() = default;

AutoPtr<Session> Session::instance;

Session& Session::getInstance() {
    if (!instance) {
        instance = makeAuto<Session>();
    }
    return *instance;
}

void Session::registerBenchmark(const SharedPtr<Unit>& benchmark, const std::string& groupName) {
    for (SharedPtr<Unit>& b : benchmarks) {
        if (b->getName() == benchmark->getName()) {
            status = "Benchmark " + b->getName() + " defined more than once";
            return;
        }
    }
    benchmarks.push(benchmark);
    Group& group = this->getGroupByName(groupName);
    group.addBenchmark(benchmark);
}

void Session::run(int argc, char* argv[]) {
    Outcome result = this->parseArgs(argc, argv);
    if (!result) {
        this->logError(result.error());
        return;
    }
#ifdef SPH_DEBUG
    this->log("Warning: running benchmark in debugging build");
#endif
    if (!status) {
        this->logError(status.error());
        return;
    }

    Baseline baseline;
    if (params.flags.has(Flag::MAKE_BASELINE)) {
        params.target.mode = Mode::MAKE_BASELINE;
        params.baseline = this->getBaselinePath();
        removePath(params.baseline);
    } else if (params.flags.has(Flag::RUN_AGAINST_BASELINE)) {
        params.target.mode = Mode::RUN_AGAINST_BASELINE;
        if (!baseline.parse(this->getBaselinePath())) {
            this->logError("Invalid baseline format");
            return;
        }
    }

    for (SharedPtr<Unit>& b : benchmarks) {
        try {
            if (params.flags.has(Flag::RUN_AGAINST_BASELINE)) {
                if (!baseline.isRecorded(b->getName())) {
                    params.target.iterateCnt = 1;
                } else {
                    params.target.iterateCnt = baseline[b->getName()].iterateCnt;
                }
            }
            Expected<Result> act = b->run(params.target);
            if (!act) {
                this->logError("Fail");
            } else {
                if (act->duration > params.maxAllowedDuration) {
                    this->log(
                        "Warning: benchmark ", b->getName(), " is takes too much time, t = ", act->duration);
                }
                if (params.flags.has(Flag::MAKE_BASELINE)) {
                    this->log(b->getName(),
                        " completed in ",
                        act->duration,
                        " ms (",
                        act->iterateCnt,
                        " iterations)");
                    this->log("   ",
                        act->mean,
                        " +- ",
                        sqrt(act->variance),
                        " (min. ",
                        act->min,
                        ", max. ",
                        act->max,
                        ")");
                    FileLogger logger(params.baseline, FileLogger::Options::APPEND);
                    logger.write(b->getName(),
                        " / ",
                        act->duration,
                        ", ",
                        act->iterateCnt,
                        ", ",
                        act->mean,
                        ", ",
                        act->variance,
                        ", ",
                        act->min,
                        ", ",
                        act->max);
                } else if (params.flags.has(Flag::RUN_AGAINST_BASELINE)) {
                    if (baseline.isRecorded(b->getName())) {
                        Result result = baseline[b->getName()];
                        ASSERT(result.iterateCnt == act->iterateCnt, result.iterateCnt, act->iterateCnt);
                        this->log(b->getName() + " ran " + std::to_string(act->iterateCnt) + " iterations");
                        this->compareResults(act.value(), result);
                    }
                }
            }
        } catch (std::exception& e) {
            this->logError("Exception caught in benchmark " + b->getName() + ":\n" + e.what());
            return;
        }
    }
}

Path Session::getBaselinePath() {
    /// \todo mode these paths to some config
    Expected<std::string> sha = getGitCommit(Path("../../src/"));
    if (!sha) {
        this->logError("Cannot determine git commit SHA");
        return Path("");
    }
    this->log("Creating baseline for commit " + sha.value());
    return Path("../../baseline/") / Path(sha.value());
}

void Session::compareResults(const Result& measured, const Result& baseline) {
    const Float diff = measured.mean - baseline.mean;
    const Float sigma = params.confidence * sqrt(measured.variance + baseline.variance);
    if (diff < -sigma) {
        Console::ScopedColor color(Console::Foreground::GREEN);
        this->log(measured.duration, " < ", baseline.duration);
    } else if (diff > sigma) {
        Console::ScopedColor color(Console::Foreground::RED);
        this->log(measured.duration, " > ", baseline.duration);
    } else {
        Console::ScopedColor color(Console::Foreground::LIGHT_GRAY);
        this->log(measured.duration, " == ", baseline.duration);
    }
}

Group& Session::getGroupByName(const std::string& groupName) {
    for (Group& group : groups) {
        if (group.getName() == groupName) {
            return group;
        }
    }
    // if not found, create a new one
    groups.emplaceBack(groupName);
    return groups[groups.size() - 1];
}

Outcome Session::parseArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) { // first arg is path to the executable
        std::string arg = argv[i];
        if (arg == "-b") {
            params.flags.set(Flag::MAKE_BASELINE);
        } else if (arg == "-r") {
            params.flags.set(Flag::RUN_AGAINST_BASELINE);
        }
        if (arg == "--help") {
            this->printHelp();
            return ""; // empty error message to quit the program
        }
    }
    return SUCCESS;
}

void Session::printHelp() {
    logger->write("Benchmark. Options:\n -b  Create baseline");
}

template <typename... TArgs>
void Session::log(TArgs&&... args) {
    if (!params.flags.has(Flag::SILENT)) {
        logger->write(std::forward<TArgs>(args)...);
    }
}

template <typename... TArgs>
void Session::logError(TArgs&&... args) {
    Console::ScopedColor bg(Console::Background::RED);
    Console::ScopedColor fg(Console::Foreground::WHITE);
    logger->write(std::forward<TArgs>(args)...);
}

Register::Register(const SharedPtr<Unit>& benchmark, const std::string& groupName) {
    Session::getInstance().registerBenchmark(benchmark, groupName);
}

NAMESPACE_BENCHMARK_END
