#include "bench/Session.h"
#include "io/FileSystem.h"
#include "io/Logger.h"
#include "system/Platform.h"
#include <algorithm>

NAMESPACE_BENCHMARK_BEGIN

Session::Session() {
    logger = makeAuto<StdOutLogger>();
    logger->setPrecision(4);
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

void Session::registerBenchmark(const SharedPtr<Unit>& benchmark, const String& groupName) {
    for (SharedPtr<Unit>& b : benchmarks) {
        if (b->getName() == benchmark->getName()) {
            status = makeFailed("Benchmark " + b->getName() + " defined more than once");
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
        params.baseline.path = this->getBaselinePath();
        FileSystem::removePath(params.baseline.path);
    } else if (params.flags.has(Flag::RUN_AGAINST_BASELINE)) {
        params.target.mode = Mode::RUN_AGAINST_BASELINE;
        if (!baseline.parse(this->getBaselinePath())) {
            this->logError("Invalid baseline format");
            return;
        }
    }

    for (SharedPtr<Unit>& b : benchmarks) {
        if (!params.benchmarksToRun.empty()) {
            // check if we want to run the benchmark
            Array<String>& names = params.benchmarksToRun;
            if (std::find(names.begin(), names.end(), b->getName()) == names.end()) {
                continue;
            }
        }

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
                if (params.flags.has(Flag::RUN_AGAINST_BASELINE)) {
                    if (baseline.isRecorded(b->getName())) {
                        Result result = baseline[b->getName()];
                        SPH_ASSERT(result.iterateCnt == act->iterateCnt, result.iterateCnt, act->iterateCnt);
                        this->log(b->getName() + " ran " + toString(act->iterateCnt) + " iterations");
                        this->compareResults(act.value(), result);
                    } else {
                        this->log(b->getName() + " not recorded in the baseline");
                    }
                } else {
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
                    if (params.flags.has(Flag::MAKE_BASELINE)) {
                        this->writeBaseline(b->getName(), act.value());
                    }
                }
            }
        } catch (const std::exception& e) {
            this->logError("Exception caught in benchmark " + b->getName() + ":\n" + exceptionMessage(e));
            return;
        }
    }
}

Path Session::getBaselinePath() {
    /// \todo mode these paths to some config
    Expected<String> sha = getGitCommit(Path("."), params.baseline.commit);
    if (!sha) {
        this->logError("Cannot determine git commit SHA");
        return Path("");
    }
    this->log("Baseline for commit " + sha.value());
    return Path(L"perf-" + sha->substr(0, 8) + ".csv");
}

void Session::writeBaseline(const String& name, const Result& measured) {
    FileLogger logger(params.baseline.path, FileLogger::Options::APPEND);
    // clang-format off
    logger.write(name, ", ", measured.duration, ", ", measured.iterateCnt, ", ",
        measured.mean, ", ", measured.variance, ", ", measured.min, ", ", measured.max);
    // clang-format on
}

void Session::compareResults(const Result& measured, const Result& baseline) {
    const Float diff = measured.mean - baseline.mean;
    const Float sigma = params.confidence * sqrt(measured.variance + baseline.variance);
    if (diff < -sigma) {
        ScopedConsole color(Console::Foreground::GREEN);
        this->log(measured.duration, " < ", baseline.duration);
    } else if (diff > sigma) {
        ScopedConsole color(Console::Foreground::RED);
        this->log(measured.duration, " > ", baseline.duration);
    } else {
        ScopedConsole color(Console::Foreground::LIGHT_GRAY);
        this->log(measured.duration, " == ", baseline.duration);
    }
}

Group& Session::getGroupByName(const String& groupName) {
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
        String arg = String::fromAscii(argv[i]);
        SPH_ASSERT(!arg.empty());
        if (arg == "-b") {
            params.flags.set(Flag::MAKE_BASELINE);
        } else if (arg == "-r") {
            params.flags.set(Flag::RUN_AGAINST_BASELINE);
            if (i < argc - 1) {
                params.baseline.commit = std::stoi(argv[i + 1]);
                i++;
            }
        } else if (arg == "--help") {
            this->printHelp();
            return Outcome(""); // empty error message to quit the program
        } else if (arg[0] != '-') {
            // not starting with '-', it must be a name of the benchmark or group to run
            if (arg[0] == '[' && arg[arg.size() - 1] == ']') {
                // find the group by name
                Group& group = this->getGroupByName(arg);
                for (auto& unit : group) {
                    params.benchmarksToRun.push(unit->getName());
                }
            } else {
                // single benchmark
                if (arg[0] == '\"' && arg[arg.size() - 1] == '\"') {
                    // trim the name
                    arg = arg.substr(1, arg.size() - 2);
                }
                params.benchmarksToRun.push(arg);
            }
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
    ScopedConsole bg(Console::Background::RED);
    ScopedConsole fg(Console::Foreground::WHITE);
    logger->write(std::forward<TArgs>(args)...);
}

Register::Register(const SharedPtr<Unit>& benchmark, const String& groupName) {
    Session::getInstance().registerBenchmark(benchmark, groupName);
}

NAMESPACE_BENCHMARK_END
