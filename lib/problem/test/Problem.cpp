#include "problem/Problem.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "quantities/Storage.h"
#include "sph/initial/Initial.h"
#include "sph/timestepping/TimeStepping.h"
#include "system/Callbacks.h"
#include "system/Output.h"
#include "utils/Approx.h"

using namespace Sph;

class DummyCallbacks : public Abstract::Callbacks {
private:
    Size& stepIdx;
    Size abortAfterStep;

public:
    DummyCallbacks(Size& stepIdx, const Size abortAfterStep = 1000)
        : stepIdx(stepIdx)
        , abortAfterStep(abortAfterStep) {
    }


    virtual void onTimeStep(const float UNUSED(progress),
        const std::shared_ptr<Storage>& UNUSED(storage)) override {
        stepIdx++;
    }

    /// Returns whether current run should be aborted or not. Can be called any time.
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
        , outputTimes(outputTimes) {
    }

    virtual std::string dump(Storage& UNUSED(storage), const Float time) override {
        outputTimes.push(time);
        return "";
    }

    virtual Outcome load(const std::string& UNUSED(path), Storage& UNUSED(storage)) override {
        NOT_IMPLEMENTED;
    }
};

static std::unique_ptr<Problem> setupProblem() {
    GlobalSettings settings;
    settings.set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 0.1_f + EPS);
    settings.set(GlobalSettingsIds::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::NONE);
    settings.set(GlobalSettingsIds::RUN_TIME_RANGE, Range(0._f, 1._f));
    settings.set(GlobalSettingsIds::RUN_OUTPUT_INTERVAL, 0.21_f);
    settings.set(GlobalSettingsIds::RUN_LOGGER, LoggerEnum::NONE);
    std::unique_ptr<Problem> problem = std::make_unique<Problem>(settings, std::make_shared<Storage>());
    InitialConditions conds(problem->storage, settings);
    BodySettings bodySettings;
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 10);
    conds.addBody(SphericalDomain(Vector(0._f), 1._f), bodySettings);
    return problem;
}

TEST_CASE("Problem run", "[problem]") {
    std::unique_ptr<Problem> problem = setupProblem();
    Size stepIdx = 0;
    /// \todo insert custom timestepping and custom logger and test they are properly called
    problem->callbacks = std::make_unique<DummyCallbacks>(stepIdx);
    Array<Float> outputTimes;
    problem->output = std::make_unique<DummyOutput>(outputTimes);

    REQUIRE(stepIdx == 0);
    problem->run();
    REQUIRE(stepIdx == 10);
    REQUIRE(outputTimes.size() == 4);
    Size i = 0;
    for (Float t : outputTimes) {
        REQUIRE(t == approx(0.3_f + 0.2_f * i, 1.e-5_f));
        i++;
    }
}

TEST_CASE("Problem abort", "[problem]") {
    std::unique_ptr<Problem> problem = setupProblem();
    Size stepIdx = 0;
    problem->callbacks = std::make_unique<DummyCallbacks>(stepIdx, 6);
    problem->run();
    REQUIRE(stepIdx == 6);
}

TEST_CASE("Problem run twice", "[problem]") {
    std::unique_ptr<Problem> problem = setupProblem();
    Array<Float> outputTimes;
    problem->output = std::make_unique<DummyOutput>(outputTimes);

    problem->run();
    REQUIRE(outputTimes.size() == 4);
    problem->run();
    REQUIRE(outputTimes.size() == 8);
    Size i = 0;
    for (Float t : outputTimes) {
        REQUIRE(t == approx(0.3_f + 0.2_f * (i % 4), 1.e-5_f));
        i++;
    }
}
