#pragma once

#include "run/Worker.h"

NAMESPACE_SPH_BEGIN

class MaterialProvider {
protected:
    BodySettings body;

public:
    explicit MaterialProvider(const BodySettings& overrides = EMPTY_SETTINGS);

protected:
    void addMaterialEntries(VirtualSettings::Category& category, Function<bool()> enabler);
};

class MaterialWorker : public IMaterialWorker, public MaterialProvider {
public:
    MaterialWorker(const std::string& name, const BodySettings& overrides = EMPTY_SETTINGS);

    virtual std::string className() const override {
        return "material";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return {};
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class DisableDerivativeCriterionWorker : public IMaterialWorker {
public:
    DisableDerivativeCriterionWorker(const std::string& name)
        : IMaterialWorker(name) {}

    virtual std::string className() const override {
        return "optimize timestepping";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "material", WorkerType::MATERIAL } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

NAMESPACE_SPH_END
