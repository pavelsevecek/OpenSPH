#include "benchmark/Benchmark.h"
#include "io/Logger.h"

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
    benchmarks.push(benchmark);
    Group& group = this->getGroupByName(groupName);
    group.addBenchmark(benchmark);
}

Outcome Session::run(int argc, char* argv[]) {
    Outcome result = this->parseArgs(argc, argv);
    if (!result) {
        return result;
    }
    for (SharedPtr<Unit>& b : benchmarks) {
        try {
            Stats stats;
            Size elapsed;
            b->run(stats, elapsed);
            this->log(b->getName() + " completed in " + std::to_string(elapsed));
        } catch (std::exception& e) {
            return e.what();
        }
    }
    return SUCCESS;
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

Outcome Session::parseArgs(int UNUSED(arcs), char* UNUSED(argv)[]) {
    /// \todo
    return SUCCESS;
}

void Session::log(const std::string& text) {
    if (!params.flags.has(Flag::SILENT)) {
        logger->write(text);
    }
}

Register::Register(const SharedPtr<Unit>& benchmark, const std::string& groupName) {
    Session::getInstance().registerBenchmark(benchmark, groupName);
}

NAMESPACE_BENCHMARK_END
