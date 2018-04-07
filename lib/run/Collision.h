#pragma once

#include "run/CompositeRun.h"
#include "sph/initial/Presets.h"

NAMESPACE_SPH_BEGIN

class StabilizationRunPhase : public IRunPhase {
private:
    Presets::CollisionParams params;
    SharedPtr<Presets::Collision> data;

public:
    explicit StabilizationRunPhase(Presets::CollisionParams params);

    virtual void setUp() override;

    virtual AutoPtr<IRunPhase> getNextPhase() const override;

    virtual void handoff(Storage&& UNUSED(input)) override {
        STOP;
    }

private:
    virtual void tearDown(const Statistics& UNUSED(stats)) override {}
};

class FragmentationRunPhase : public IRunPhase {
private:
    Presets::CollisionParams params;
    SharedPtr<Presets::Collision> data;

public:
    explicit FragmentationRunPhase(Presets::CollisionParams params, SharedPtr<Presets::Collision> data);

    virtual void setUp() override {
        STOP;
    }

    virtual AutoPtr<IRunPhase> getNextPhase() const override {
        return nullptr;
    }

    virtual void handoff(Storage&& input) override;

private:
    virtual void tearDown(const Statistics& stats) override;
};

class CollisionRun : public CompositeRun {
public:
    explicit CollisionRun(const Presets::CollisionParams params)
        : CompositeRun(makeAuto<StabilizationRunPhase>(params)) {}
};

NAMESPACE_SPH_END
