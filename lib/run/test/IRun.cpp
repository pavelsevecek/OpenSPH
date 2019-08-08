#include "run/IRun.h"
#include "catch.hpp"
#include "io/Output.h"
#include "objects/geometry/Domain.h"
#include "quantities/Storage.h"
#include "sph/initial/Initial.h"
#include "system/Statistics.h"
#include "tests/Approx.h"
#include "thread/Pool.h"
#include "timestepping/TimeStepping.h"
#include "utils/Utils.h"

using namespace Sph;

class DummyCallbacks : public IRunCallbacks {
public:
    Size stepIdx = 0;
    Size abortAfterStep = 1000;


    virtual void onSetUp(const Storage& UNUSED(storage), Statistics& UNUSED(stats)) override {}

    virtual void onTimeStep(const Storage& UNUSED(storage), Statistics& UNUSED(stats)) override {
        stepIdx++;
    }

    virtual bool shouldAbortRun() const override {
        return (stepIdx >= abortAfterStep);
    }
};

class DummyOutput : public IOutput {
private:
    Array<Float>& outputTimes;

public:
    DummyOutput(Array<Float>& outputTimes)
        : IOutput(Path("%d"))
        , outputTimes(outputTimes) {}

    virtual Expected<Path> dump(const Storage& UNUSED(storage), const Statistics& stats) override {
        outputTimes.push(stats.get<Float>(StatisticsId::RUN_TIME));
        return Path();
    }
};

namespace {

class TestRun : public IRun {
public:
    Array<Float> outputTimes; // times where output was called
    bool runEnded = false;

    TestRun() {
        settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.1_f + EPS);
        settings.set(RunSettingsId::TIMESTEPPING_CRITERION, EMPTY_FLAGS);
        settings.set(RunSettingsId::RUN_END_TIME, 1._f);
        settings.set(RunSettingsId::RUN_OUTPUT_INTERVAL, 0.21_f);
        settings.set(RunSettingsId::RUN_LOGGER, LoggerEnum::NONE);
    }

    virtual void setUp(SharedPtr<Storage> storage) override {
        InitialConditions conds(settings);
        BodySettings bodySettings;
        bodySettings.set(BodySettingsId::PARTICLE_COUNT, 10);
        conds.addMonolithicBody(*storage, SphericalDomain(Vector(0._f), 1._f), bodySettings);

        /// \todo insert custom timestepping and custom logger and test they are properly called
        outputTimes.clear();
        this->output = makeAuto<DummyOutput>(outputTimes);
    }

protected:
    virtual void tearDown(const Storage& UNUSED(storage), const Statistics& UNUSED(stats)) override {
        runEnded = true;
    }
};
} // namespace

TEST_CASE("Simple run", "[run]") {
    TestRun run;
    Storage storage;
    DummyCallbacks callbacks;
    REQUIRE_NOTHROW(run.run(storage, callbacks));
    REQUIRE(callbacks.stepIdx == 10);
    REQUIRE(run.runEnded);
    REQUIRE(run.outputTimes.size() == 5);
    Size i = 0;
    for (Float t : run.outputTimes) {
        if (i == 0) {
            // first output is at t=0 (basically stored initial conditions)
            REQUIRE(t == 0._f);
        } else {
            REQUIRE(t == approx(0.1_f + 0.2_f * i, 1.e-5_f));
        }
        i++;
    }
}

TEST_CASE("Run abort", "[run]") {
    TestRun run;
    DummyCallbacks callbacks;
    callbacks.abortAfterStep = 6; // abort after 6th step
    Storage storage;
    REQUIRE_NOTHROW(run.run(storage, callbacks));
    REQUIRE(callbacks.stepIdx == 6);
    REQUIRE(run.runEnded);
}

TEST_CASE("Run twice", "[run]") {
    TestRun run;
    DummyCallbacks callbacks;
    Storage storage;
    REQUIRE_NOTHROW(run.run(storage, callbacks));
    REQUIRE(run.outputTimes.size() == 5);

    REQUIRE_NOTHROW(run.run(storage, callbacks));
    REQUIRE(run.outputTimes.size() == 5);
    Size i = 0;
    for (Float t : run.outputTimes) {
        if (i == 0) {
            REQUIRE(t == 0._f);
        } else {
            REQUIRE(t == approx(0.1_f + 0.2_f * i, 1.e-5_f));
        }
        i++;
    }
}
