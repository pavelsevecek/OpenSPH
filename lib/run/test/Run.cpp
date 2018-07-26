#include "catch.hpp"
#include "io/Output.h"
#include "objects/geometry/Domain.h"
#include "quantities/Storage.h"
#include "run/IRun.h"
#include "run/RunCallbacks.h"
#include "sph/initial/Initial.h"
#include "system/Statistics.h"
#include "tests/Approx.h"
#include "thread/Pool.h"
#include "timestepping/TimeStepping.h"
#include "utils/Utils.h"

using namespace Sph;

class DummyCallbacks : public IRunCallbacks {
private:
    Size& stepIdx;
    Size abortAfterStep;
    bool& runEnded;

public:
    DummyCallbacks(Size& stepIdx, bool& runEnded, const Size abortAfterStep = 1000)
        : stepIdx(stepIdx)
        , abortAfterStep(abortAfterStep)
        , runEnded(runEnded) {}


    virtual void onTimeStep(const Storage& UNUSED(storage), Statistics& UNUSED(stats)) override {
        stepIdx++;
    }

    virtual void onRunStart(const Storage& UNUSED(storage), Statistics& UNUSED(stats)) override {}

    virtual void onRunEnd(const Storage& UNUSED(storage), Statistics& UNUSED(stats)) override {
        runEnded = true;
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

    virtual Path dump(Storage& UNUSED(storage), const Statistics& stats) override {
        outputTimes.push(stats.get<Float>(StatisticsId::RUN_TIME));
        return Path();
    }

    virtual Outcome load(const Path& UNUSED(path),
        Storage& UNUSED(storage),
        Statistics& UNUSED(stats)) override {
        NOT_IMPLEMENTED;
    }
};

namespace {

class TestRun : public IRun {
public:
    Array<Float> outputTimes; // times where output was called
    Size stepIdx;             // current timestep index
    bool runEnded;            // set to true by DummyCallbacks when run ends
    Size terminateAfterOutput;


    TestRun(const Size terminateAfterOutput = 1000)
        : terminateAfterOutput(terminateAfterOutput) {
        settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.1_f + EPS);
        settings.set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::NONE);
        settings.set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 1._f));
        settings.set(RunSettingsId::RUN_OUTPUT_INTERVAL, 0.21_f);
        settings.set(RunSettingsId::RUN_LOGGER, LoggerEnum::NONE);
    }

    virtual void setUp() override {
        storage = makeShared<Storage>();
        scheduler = ThreadPool::getGlobalInstance();
        InitialConditions conds(*scheduler, settings);
        BodySettings bodySettings;
        bodySettings.set(BodySettingsId::PARTICLE_COUNT, 10);
        conds.addMonolithicBody(*storage, SphericalDomain(Vector(0._f), 1._f), bodySettings);
        stepIdx = 0;
        runEnded = false;

        /// \todo insert custom timestepping and custom logger and test they are properly called
        this->callbacks = makeAuto<DummyCallbacks>(stepIdx, runEnded, terminateAfterOutput);
        outputTimes.clear();
        this->output = makeAuto<DummyOutput>(outputTimes);
    }

protected:
    virtual void tearDown(const Statistics& UNUSED(stats)) override {}
};
} // namespace

TEST_CASE("Simple run", "[run]") {
    TestRun run;
    REQUIRE_NOTHROW(run.setUp());
    REQUIRE_NOTHROW(run.run());
    REQUIRE(run.stepIdx == 10);
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
    TestRun run(6); // abort after 6th step
    REQUIRE_NOTHROW(run.setUp());
    REQUIRE_NOTHROW(run.run());
    REQUIRE(run.stepIdx == 6);
    REQUIRE(run.runEnded);
}

TEST_CASE("Run twice", "[run]") {
    TestRun run;
    REQUIRE_NOTHROW(run.setUp());
    REQUIRE_NOTHROW(run.run());
    REQUIRE(run.outputTimes.size() == 5);
    REQUIRE_NOTHROW(run.setUp());
    REQUIRE_NOTHROW(run.run());
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

TEST_CASE("Run without setup", "[run]") {
    TestRun run;
    REQUIRE_ASSERT(run.run());
    run.setUp();
    REQUIRE_NOTHROW(run.run());
    /// \todo check this with assert REQUIRE_ASSERT(run.run());
}
