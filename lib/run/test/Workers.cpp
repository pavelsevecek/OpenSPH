#include "catch.hpp"
#include "io/FileSystem.h"
#include "io/Output.h"
#include "run/Node.h"
#include "run/workers/InitialConditionWorkers.h"
#include "run/workers/Presets.h"
#include "run/workers/SimulationWorkers.h"
#include "tests/Setup.h"
#include "utils/Utils.h"

using namespace Sph;

class TestCreateParticles : public IParticleWorker {
private:
    Float startTime = 0._f;

public:
    TestCreateParticles(const std::string& name, const Float startTime)
        : IParticleWorker(name)
        , startTime(startTime) {}

    virtual std::string className() const override {
        return "create particles";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return {};
    }

    virtual VirtualSettings getSettings() override {
        return {};
    }

    virtual void evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) override {
        result = makeShared<ParticleData>();
        result->storage = Tests::getSolidStorage(1000, BodySettings::getDefaults(), 1.e6_f);
        result->overrides.set(RunSettingsId::RUN_START_TIME, startTime);
    }
};

class TestWorkerCallbacks : public IWorkerCallbacks {
private:
    Float expectedSetUpTime;
    bool setUpCalled = false;

public:
    TestWorkerCallbacks(const Float expectedSetUpTime)
        : expectedSetUpTime(expectedSetUpTime) {}

    bool wasSetUpCalled() const {
        return setUpCalled;
    }

    virtual void onStart(const IWorker& UNUSED(worker)) override {}

    virtual void onEnd(const Storage& UNUSED(storage), const Statistics& UNUSED(stats)) override {}

    virtual void onSetUp(const Storage&, Statistics& stats) override {
        REQUIRE(stats.has(StatisticsId::RUN_TIME));
        REQUIRE(stats.get<Float>(StatisticsId::RUN_TIME) == expectedSetUpTime);

        setUpCalled = true;
    }

    virtual void onTimeStep(const Storage&, Statistics&) override {}

    virtual bool shouldAbortRun() const override {
        return false;
    }
};

TEMPLATE_TEST_CASE("New run", "[worker]", SphWorker, SphStabilizationWorker, NBodyWorker) {
    SharedPtr<WorkerNode> runNode = makeNode<TestType>("simulation");
    SharedPtr<WorkerNode> icNode = makeNode<TestCreateParticles>("ic", 0._f);
    icNode->connect(runNode, "particles");

    VirtualSettings settings = runNode->getSettings();
    settings.set("is_resumed", false);
    settings.set("run.end_time", 1._f);

    TestWorkerCallbacks callbacks(0._f);
    RunSettings overrides = EMPTY_SETTINGS;
    overrides.set(RunSettingsId::RUN_LOGGER, LoggerEnum::NONE);
    REQUIRE_NOTHROW(runNode->run(overrides, callbacks));
    REQUIRE(callbacks.wasSetUpCalled());
}

TEMPLATE_TEST_CASE("Resumed run", "[worker]", SphWorker, SphStabilizationWorker, NBodyWorker) {
    const Float startTime = 20._f;
    SharedPtr<WorkerNode> runNode = makeNode<TestType>("simulation");
    SharedPtr<WorkerNode> icNode = makeNode<TestCreateParticles>("ic", startTime);
    icNode->connect(runNode, "particles");

    VirtualSettings settings = runNode->getSettings();
    settings.set("is_resumed", true);
    settings.set("run.end_time", 21._f);

    TestWorkerCallbacks callbacks(startTime);
    RunSettings overrides = EMPTY_SETTINGS;
    overrides.set(RunSettingsId::RUN_LOGGER, LoggerEnum::NONE);
    REQUIRE_NOTHROW(runNode->run(overrides, callbacks));
    REQUIRE(callbacks.wasSetUpCalled());
}

TEST_CASE("Simple collision run", "[worker]") {
    UniqueNameManager mgr;
    SharedPtr<WorkerNode> node = Presets::makeAsteroidCollision(mgr, 100);

    // just test that everything runs without exceptions/asserts
    RunSettings overrides = EMPTY_SETTINGS;
    overrides.set(RunSettingsId::RUN_END_TIME, EPS).set(RunSettingsId::RUN_LOGGER, LoggerEnum::NONE);
    NullWorkerCallbacks callbacks;
    REQUIRE_NOTHROW(node->run(overrides, callbacks));
}

TEST_CASE("Fragmentation reaccumulation run", "[worker]") {
    UniqueNameManager mgr;
    SharedPtr<WorkerNode> node = Presets::makeFragmentationAndReaccumulation(mgr, 100);

    // just test that everything runs without exceptions/asserts
    RunSettings overrides = EMPTY_SETTINGS;
    overrides.set(RunSettingsId::RUN_END_TIME, EPS).set(RunSettingsId::RUN_LOGGER, LoggerEnum::NONE);
    NullWorkerCallbacks callbacks;
    REQUIRE_NOTHROW(node->run(overrides, callbacks));
}
