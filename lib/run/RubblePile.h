#pragma once

#include "run/CompositeRun.h"
#include "sph/initial/Presets.h"

NAMESPACE_SPH_BEGIN

class RubblePileRunPhase : public IRunPhase {
private:
    Presets::CollisionParams params;

public:
    explicit RubblePileRunPhase(const Presets::CollisionParams params, SharedPtr<IRunCallbacks> callbacks);

    virtual void setUp() override;

    virtual void handoff(Storage&& input) override;

    virtual AutoPtr<IRunPhase> getNextPhase() const override;

private:
    virtual void tearDown(const Statistics& stats) override;
};

NAMESPACE_SPH_END
