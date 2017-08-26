#include "run/IRun.h"
#include "catch.hpp"
#include "objects/geometry/Domain.h"
#include "io/Output.h"
#include "quantities/Storage.h"
#include "sph/initial/Initial.h"
#include "system/Callbacks.h"
#include "system/Statistics.h"
#include "timestepping/TimeStepping.h"
#include "tests/Approx.h"
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
        outputTimes.push(stats.get<Float>(StatisticsId::TOTAL_TIME));
        return Path();
    }

    virtual Outcome load(const Path& UNUSED(path), Storage& UNUSED(storage)) override {
        NOT_IMPLEMENTED;
    }
};


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
        InitialConditions conds(*storage, settings);
        BodySettings bodySettings;
        bodySettings.set(BodySettingsId::PARTICLE_COUNT, 10);
        conds.addBody(SphericalDomain(Vector(0._f), 1._f), bodySettings);
        stepIdx = 0;
        runEnded = false;

        /// \todo insert custom timestepping and custom logger and test they are properly called
        this->callbacks = makeAuto<DummyCallbacks>(stepIdx, runEnded, terminateAfterOutput);
        outputTimes.clear();
        this->output = makeAuto<DummyOutput>(outputTimes);
    }

protected:
    virtual void tearDown() override {}
};

TEST_CASE("Simple run", "[run]") {
    TestRun run;
    REQUIRE_NOTHROW(run.setUp());
    REQUIRE_NOTHROW(run.run());
    REQUIRE(run.stepIdx == 10);
    REQUIRE(run.runEnded);
    REQUIRE(run.outputTimes.size() == 4);
    Size i = 0;
    for (Float t : run.outputTimes) {
        REQUIRE(t == approx(0.3_f + 0.2_f * i, 1.e-5_f));
        i++;
    }
}

TEST_CASE("Run abort", "[run]") {
    TestRun run(6); // abort after 6th step
    REQUIRE_NOTHROW(run.setUp());
    REQUIRE_NOTHROW(run.run());
    REQUIRE(run.stepIdx == 6);
    REQUIRE_FALSE(run.runEnded); // this is only set if the run ends successfully
}

TEST_CASE("Run twice", "[run]") {
    TestRun run;
    REQUIRE_NOTHROW(run.setUp());
    REQUIRE_NOTHROW(run.run());
    REQUIRE(run.outputTimes.size() == 4);
    REQUIRE_NOTHROW(run.setUp());
    REQUIRE_NOTHROW(run.run());
    REQUIRE(run.outputTimes.size() == 4);
    Size i = 0;
    for (Float t : run.outputTimes) {
        REQUIRE(t == approx(0.3_f + 0.2_f * i, 1.e-5_f));
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
