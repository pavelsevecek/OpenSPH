#include "catch.hpp"
#include "io/FileSystem.h"
#include "io/Output.h"
#include "run/Node.h"
#include "run/jobs/GeometryJobs.h"
#include "run/jobs/InitialConditionJobs.h"
#include "run/jobs/IoJobs.h"
#include "run/jobs/MaterialJobs.h"
#include "run/jobs/ParticleJobs.h"
#include "run/jobs/Presets.h"
#include "run/jobs/ScriptJobs.h"
#include "run/jobs/SimulationJobs.h"
#include "tests/Setup.h"
#include "utils/Utils.h"

using namespace Sph;

class TestCreateParticles : public IParticleJob {
private:
    Float startTime = 0._f;

public:
    TestCreateParticles(const std::string& name, const Float startTime)
        : IParticleJob(name)
        , startTime(startTime) {}

    virtual std::string className() const override {
        return "create particles";
    }

    virtual UnorderedMap<std::string, ExtJobType> getSlots() const override {
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

class TestJobCallbacks : public IJobCallbacks {
private:
    Float expectedSetUpTime;
    bool setUpCalled = false;

public:
    TestJobCallbacks(const Float expectedSetUpTime)
        : expectedSetUpTime(expectedSetUpTime) {}

    bool wasSetUpCalled() const {
        return setUpCalled;
    }

    virtual void onStart(const IJob& UNUSED(job)) override {}

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

TEMPLATE_TEST_CASE("New run", "[job]", SphJob, SphStabilizationJob, NBodyJob) {
    SharedPtr<JobNode> runNode = makeNode<TestType>("simulation");
    SharedPtr<JobNode> icNode = makeNode<TestCreateParticles>("ic", 0._f);
    icNode->connect(runNode, "particles");

    VirtualSettings settings = runNode->getSettings();
    settings.set("is_resumed", false);
    settings.set("run.end_time", 1._f);

    TestJobCallbacks callbacks(0._f);
    RunSettings overrides = EMPTY_SETTINGS;
    overrides.set(RunSettingsId::RUN_LOGGER, LoggerEnum::NONE);
    REQUIRE_NOTHROW(runNode->run(overrides, callbacks));
    REQUIRE(callbacks.wasSetUpCalled());
}

TEMPLATE_TEST_CASE("Resumed run", "[job]", SphJob, SphStabilizationJob, NBodyJob) {
    const Float startTime = 20._f;
    SharedPtr<JobNode> runNode = makeNode<TestType>("simulation");
    SharedPtr<JobNode> icNode = makeNode<TestCreateParticles>("ic", startTime);
    icNode->connect(runNode, "particles");

    VirtualSettings settings = runNode->getSettings();
    settings.set("is_resumed", true);
    settings.set("run.end_time", 21._f);

    TestJobCallbacks callbacks(startTime);
    RunSettings overrides = EMPTY_SETTINGS;
    overrides.set(RunSettingsId::RUN_LOGGER, LoggerEnum::NONE);
    REQUIRE_NOTHROW(runNode->run(overrides, callbacks));
    REQUIRE(callbacks.wasSetUpCalled());
}

TEST_CASE("Preset runs", "[job]") {
    UniqueNameManager mgr;
    for (Presets::Id id : EnumMap::getAll<Presets::Id>()) {
        INFO("Testing preset " + EnumMap::toString(id));
        SharedPtr<JobNode> node = Presets::make(id, mgr, 100);

        // just test that everything runs without exceptions/asserts
        RunSettings globals = EMPTY_SETTINGS;
        globals.set(RunSettingsId::RUN_END_TIME, EPS)
            .set(RunSettingsId::RUN_LOGGER, LoggerEnum::NONE)
            .set(RunSettingsId::RUN_RNG, RngEnum::BENZ_ASPHAUG)
            .set(RunSettingsId::RUN_RNG_SEED, 1234)
            .set(RunSettingsId::RUN_THREAD_CNT, 0)
            .set(RunSettingsId::RUN_THREAD_GRANULARITY, 20)
            .set(RunSettingsId::SPH_KERNEL, KernelEnum::CUBIC_SPLINE)
            .set(RunSettingsId::GENERATE_UVWS, false);
        NullJobCallbacks callbacks;
        REQUIRE_NOTHROW(node->run(globals, callbacks));
    }
}

class TestProc : public VirtualSettings::IEntryProc {
public:
    virtual void onCategory(const std::string& UNUSED(name)) const override {}

    virtual void onEntry(const std::string& UNUSED(key), IVirtualEntry& entry) const override {
        // check self-consistency
        if (!entry.isValid(entry.get())) {
            throw InvalidSetup("Entry '" + entry.getName() + "' not valid.");
        }

        if (entry.getType() == IVirtualEntry::Type::PATH) {
            if (!entry.getPathType()) {
                throw InvalidSetup("Entry '" + entry.getName() + "' has no assigned path type.");
            }
        }
    }
};

static void registerJobs() {
    static SphJob sSph("");
    static CollisionGeometrySetup sSetup("");
    static MonolithicBodyIc sIc("");
    static SaveFileJob sIo("");
    static BlockJob sBlock("");
    static MaterialJob sMat("");

#ifdef SPH_USE_CHAISCRIPT
    static ChaiScriptJob sScript("");
#endif
}

TEST_CASE("Check registered jobs", "[job]") {
    registerJobs();

    ArrayView<const AutoPtr<IJobDesc>> jobDescs = enumerateRegisteredJobs();
    for (const AutoPtr<IJobDesc>& desc : jobDescs) {
        AutoPtr<IJob> job = desc->create(NOTHING);
        VirtualSettings settings = job->getSettings();
        TestProc proc;
        REQUIRE_NOTHROW(settings.enumerate(proc));
    }
}
