#include "run/Run.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "io/Output.h"
#include "quantities/Storage.h"
#include "sph/initial/Initial.h"
#include "system/Callbacks.h"
#include "timestepping/TimeStepping.h"
#include "utils/Approx.h"
#include "utils/Utils.h"

using namespace Sph;

class DummyCallbacks : public Abstract::Callbacks {
private:
    Size& stepIdx;
    Size abortAfterStep;
    bool& runEnded;

public:
    DummyCallbacks(Size& stepIdx, bool& runEnded, const Size abortAfterStep = 1000)
        : stepIdx(stepIdx)
        , abortAfterStep(abortAfterStep)
        , runEnded(runEnded) {}


    virtual void onTimeStep(const std::shared_ptr<Storage>& UNUSED(storage),
        Statistics& UNUSED(stats)) override {
        stepIdx++;
    }

    virtual void onRunStart(const std::shared_ptr<Storage>& UNUSED(storage),
        Statistics& UNUSED(stats)) override {}

    virtual void onRunEnd(const std::shared_ptr<Storage>& UNUSED(storage),
        Statistics& UNUSED(stats)) override {
        runEnded = true;
    }

    virtual bool shouldAbortRun() const override {
        return (stepIdx >= abortAfterStep);
    }
};

class DummyOutput : public Abstract::Output {
private:
    Array<Float>& outputTimes;

public:
    DummyOutput(Array<Float>& outputTimes)
        : Abstract::Output("%d")
        , outputTimes(outputTimes) {}

    virtual std::string dump(Storage& UNUSED(storage), const Float time) override {
        outputTimes.push(time);
        return "";
    }

    virtual Outcome load(const std::string& UNUSED(path), Storage& UNUSED(storage)) override {
        NOT_IMPLEMENTED;
    }
};


class TestRun : public Abstract::Run {
public:
    Array<Float> outputTimes; // times where output was called
    Size stepIdx;             // current timestep index
    bool runEnded;            // set to true by DummyCallbacks when run ends
    Size terminateAfterOutput;


    TestRun(const Size terminateAfterOutput = 1000)
        : terminateAfterOutput(terminateAfterOutput) {
        settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.1_f + EPS);
        settings.set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::NONE);
        settings.set(RunSettingsId::RUN_TIME_RANGE, Range(0._f, 1._f));
        settings.set(RunSettingsId::RUN_OUTPUT_INTERVAL, 0.21_f);
        settings.set(RunSettingsId::RUN_LOGGER, LoggerEnum::NONE);
    }

    virtual std::shared_ptr<Storage> setUp() override {
        storage = std::make_shared<Storage>();
        InitialConditions conds(*storage, settings);
        BodySettings bodySettings;
        bodySettings.set(BodySettingsId::PARTICLE_COUNT, 10);
        conds.addBody(SphericalDomain(Vector(0._f), 1._f), bodySettings);
        stepIdx = 0;
        runEnded = false;

        /// \todo insert custom timestepping and custom logger and test they are properly called
        this->callbacks = std::make_unique<DummyCallbacks>(stepIdx, runEnded, terminateAfterOutput);
        outputTimes.clear();
        this->output = std::make_unique<DummyOutput>(outputTimes);
        return storage;
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
    REQUIRE(run.runEnded);
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
    REQUIRE_ASSERT(run.run());
}
