#include "bench/Session.h"
#include "io/FileSystem.h"
#include "io/Logger.h"
#include "system/Platform.h"

NAMESPACE_BENCHMARK_BEGIN

Session::Session() {
    logger = makeAuto<StdOutLogger>();
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
    if (params.flags.has(Flag::MAKE_BASELINE)) {
        /// \todo mode these paths to some config
        Expected<std::string> sha = getGitCommit(Path("../../src/"));
        if (!sha) {
            this->logError("Cannot determine git commit SHA");
            return;
        }
        this->log("Creating baseline for commit " + sha.value());
        result = createDirectory(Path("../../baseline/") / Path(sha.value()));
        if (!result) {
            this->logError(result.error());
            return;
        }
    }

    for (SharedPtr<Unit>& b : benchmarks) {
        try {
            Stats stats;
            Size elapsed;
            b->run(stats, elapsed);
            this->log(b->getName() + " completed in " + std::to_string(elapsed) + " ms (" +
                      std::to_string(stats.count()) + " iterations)");
        } catch (std::exception& e) {
            this->logError("Exception caught in benchmark " + b->getName() + ":\n" + e.what());
            return;
        }
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

void Session::log(const std::string& text) {
    if (!params.flags.has(Flag::SILENT)) {
        logger->write(text);
    }
}

void Session::logError(const std::string& text) {
    logger->write(text);
}

Register::Register(const SharedPtr<Unit>& benchmark, const std::string& groupName) {
    Session::getInstance().registerBenchmark(benchmark, groupName);
}

NAMESPACE_BENCHMARK_END
