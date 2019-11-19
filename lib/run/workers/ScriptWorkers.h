#pragma once

#include "run/Worker.h"

NAMESPACE_SPH_BEGIN

#ifdef SPH_USE_CHAISCRIPT

class ChaiScriptWorker : public IParticleWorker {
private:
    Path file = Path("script.chai");

    int inputCnt = 8; // needs to be max for proper loading ...
    StaticArray<std::string, 8> slotNames;

    int paramCnt = 8;
    StaticArray<std::string, 8> paramNames;
    StaticArray<Float, 8> paramValues;

public:
    ChaiScriptWorker(const std::string& name);

    virtual std::string className() const override {
        return "custom script";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override;

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

#endif

NAMESPACE_SPH_END
