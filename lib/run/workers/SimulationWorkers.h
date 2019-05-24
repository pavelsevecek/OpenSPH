#pragma once

#include "run/Worker.h"

NAMESPACE_SPH_BEGIN

class SphWorker : public IRunWorker {
protected:
    RunSettings settings;
    bool isResumed = false;

public:
    explicit SphWorker(const std::string& name, const RunSettings& overrides = EMPTY_SETTINGS);

    static RunSettings getDefaultSettings(const std::string& name);

    virtual std::string className() const override {
        return "SPH run";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "particles", WorkerType::PARTICLES }, { "boundary", WorkerType::GEOMETRY } };
    }

    virtual UnorderedMap<std::string, WorkerType> requires() const override {
        UnorderedMap<std::string, WorkerType> map{ { "particles", WorkerType::PARTICLES } };
        if (settings.get<BoundaryEnum>(RunSettingsId::DOMAIN_BOUNDARY) != BoundaryEnum::NONE) {
            map.insert("boundary", WorkerType::GEOMETRY);
        }
        return map;
    }

    virtual VirtualSettings getSettings() override;

    virtual AutoPtr<IRun> getRun(const RunSettings& overrides) const;
};

class SphStabilizationWorker : public SphWorker {
public:
    using SphWorker::SphWorker;

    virtual std::string className() const override {
        return "SPH stabilization";
    }

    virtual VirtualSettings getSettings() override;

    virtual AutoPtr<IRun> getRun(const RunSettings& overrides) const;
};

class NBodyWorker : public IRunWorker {
private:
    RunSettings settings;
    bool isResumed = false;

public:
    explicit NBodyWorker(const std::string& name, const RunSettings& overrides = EMPTY_SETTINGS);

    static RunSettings getDefaultSettings(const std::string& name);

    virtual std::string className() const override {
        return "N-body run";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "particles", WorkerType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual AutoPtr<IRun> getRun(const RunSettings& overrides) const override;
};

NAMESPACE_SPH_END
