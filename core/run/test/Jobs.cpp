#include "catch.hpp"
#include "io/FileManager.h"
#include "io/FileSystem.h"
#include "io/Output.h"
#include "quantities/Utility.h"
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
    TestCreateParticles(const String& name, const Float startTime)
        : IParticleJob(name)
        , startTime(startTime) {}

    virtual String className() const override {
        return "create particles";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
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

TEST_CASE("Shearing sheet IC target count", "[job]") {
    SharedPtr<JobNode> icNode = makeNode<ShearingSheetIc>("sheet");
    VirtualSettings settings = icNode->getSettings();
    settings.set("fill_mode", EnumWrapper(ShearingSheetFillMode::TARGET_COUNT));
    settings.set("particle_count", 128);
    settings.set("seed", 42);
    settings.set("box_size", Vector(20._f, 10._f, 4._f));
    settings.set("radius_min", 0.1_f);
    settings.set("radius_max", 0.2_f);
    settings.set("z_dispersion", 0.1_f);

    RunSettings globals = EMPTY_SETTINGS;
    globals.set(RunSettingsId::RUN_RNG, RngEnum::BENZ_ASPHAUG);
    NullJobCallbacks callbacks;
    REQUIRE_NOTHROW(icNode->run(globals, callbacks));

    SharedPtr<ParticleData> data = icNode->getJob()->getResult().getValue<ParticleData>();
    REQUIRE(data->storage.getParticleCnt() == 128);
    REQUIRE(data->overrides.get<BoundaryEnum>(RunSettingsId::DOMAIN_BOUNDARY) == BoundaryEnum::SHEARING_SHEET);
}

TEST_CASE("Shearing sheet IC surface density", "[job]") {
    SharedPtr<JobNode> icNode = makeNode<ShearingSheetIc>("sheet");
    VirtualSettings settings = icNode->getSettings();
    settings.set("fill_mode", EnumWrapper(ShearingSheetFillMode::TARGET_SURFACE_DENSITY));
    settings.set("surface_density", 25._f);
    settings.set("bulk_density", 5._f);
    settings.set("box_size", Vector(8._f, 5._f, 2._f));
    settings.set("radius_min", 0.2_f);
    settings.set("radius_max", 0.4_f);
    settings.set("radius_slope", -2._f);
    settings.set("seed", 7);

    RunSettings globals = EMPTY_SETTINGS;
    globals.set(RunSettingsId::RUN_RNG, RngEnum::BENZ_ASPHAUG);
    NullJobCallbacks callbacks;
    REQUIRE_NOTHROW(icNode->run(globals, callbacks));

    SharedPtr<ParticleData> data = icNode->getJob()->getResult().getValue<ParticleData>();
    const Float targetMass = 25._f * 8._f * 5._f;
    const Float totalMass = getTotalMass(data->storage);
    const ArrayView<const Float> m = data->storage.getValue<Float>(QuantityId::MASS);
    const Float maxMass = *std::max_element(m.begin(), m.end());

    REQUIRE(totalMass >= targetMass);
    REQUIRE(totalMass - targetMass <= maxMass + EPS);
}

TEST_CASE("Shearing sheet N-body smoke run", "[job]") {
    SharedPtr<JobNode> icNode = makeNode<ShearingSheetIc>("sheet");
    VirtualSettings icSettings = icNode->getSettings();
    icSettings.set("fill_mode", EnumWrapper(ShearingSheetFillMode::TARGET_COUNT));
    icSettings.set("particle_count", 24);
    icSettings.set("seed", 11);
    icSettings.set("box_size", Vector(12._f, 12._f, 2._f));
    icSettings.set("omega", 0.1_f);
    icSettings.set("dt", 0.01_f);
    icSettings.set("softening", 0._f);
    icSettings.set("radius_min", 0.05_f);
    icSettings.set("radius_max", 0.1_f);
    icSettings.set("bulk_density", 1._f);
    icSettings.set("z_dispersion", 0.01_f);

    SharedPtr<JobNode> runNode = makeNode<NBodyJob>("simulation");
    icNode->connect(runNode, "particles");

    VirtualSettings runSettings = runNode->getSettings();
    runSettings.set("is_resumed", false);
    runSettings.set(RunSettingsId::TIMESTEPPING_INTEGRATOR, EnumWrapper(TimesteppingEnum::SYMPLECTIC_EPICYCLE));
    runSettings.set(
        RunSettingsId::TIMESTEPPING_CRITERION, EnumWrapper::fromFlags(Flags<TimeStepCriterionEnum>()));
    runSettings.set(RunSettingsId::GRAVITY_CONSTANT, 1.e-6_f);
    runSettings.set(RunSettingsId::NBODY_SOFTSPHERE_ENABLE, true);
    runSettings.set("run.end_time", 0.05_f);

    RunSettings globals = EMPTY_SETTINGS;
    globals.set(RunSettingsId::RUN_LOGGER, LoggerEnum::NONE)
        .set(RunSettingsId::RUN_RNG, RngEnum::BENZ_ASPHAUG)
        .set(RunSettingsId::RUN_THREAD_CNT, 0)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 20);
    NullJobCallbacks callbacks;
    REQUIRE_NOTHROW(runNode->run(globals, callbacks));

    SharedPtr<ParticleData> data = runNode->getJob()->getResult().getValue<ParticleData>();
    REQUIRE(data->stats.has(StatisticsId::SHEARING_SHEET_SURFACE_DENSITY));
    REQUIRE(data->stats.has(StatisticsId::SHEARING_SHEET_VELOCITY_DISPERSION_X));
    REQUIRE(data->storage.getParticleCnt() > 0);
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
    virtual void onCategory(const String& UNUSED(name)) const override {}

    virtual void onEntry(const String& UNUSED(key), IVirtualEntry& entry) const override {
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
    static CollisionGeometrySetupJob sSetup("");
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
