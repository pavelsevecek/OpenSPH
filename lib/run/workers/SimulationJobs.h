#pragma once

#include "run/Job.h"

NAMESPACE_SPH_BEGIN

class SphJob : public IRunJob {
protected:
    RunSettings settings;
    bool isResumed = false;

public:
    explicit SphJob(const std::string& name, const RunSettings& overrides = EMPTY_SETTINGS);

    static RunSettings getDefaultSettings(const std::string& name);

    virtual std::string className() const override {
        return "SPH run";
    }

    virtual UnorderedMap<std::string, JobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES }, { "boundary", JobType::GEOMETRY } };
    }

    virtual UnorderedMap<std::string, JobType> requires() const override {
        UnorderedMap<std::string, JobType> map{ { "particles", JobType::PARTICLES } };
        if (settings.get<BoundaryEnum>(RunSettingsId::DOMAIN_BOUNDARY) != BoundaryEnum::NONE) {
            map.insert("boundary", JobType::GEOMETRY);
        }
        return map;
    }

    virtual VirtualSettings getSettings() override;

    virtual AutoPtr<IRun> getRun(const RunSettings& overrides) const override;
};

class SphStabilizationJob : public SphJob {
public:
    using SphJob::SphJob;

    virtual std::string className() const override {
        return "SPH stabilization";
    }

    virtual VirtualSettings getSettings() override;

    virtual AutoPtr<IRun> getRun(const RunSettings& overrides) const override;
};

class NBodyJob : public IRunJob {
private:
    RunSettings settings;
    bool isResumed = false;

public:
    explicit NBodyJob(const std::string& name, const RunSettings& overrides = EMPTY_SETTINGS);

    static RunSettings getDefaultSettings(const std::string& name);

    virtual std::string className() const override {
        return "N-body run";
    }

    virtual UnorderedMap<std::string, JobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual AutoPtr<IRun> getRun(const RunSettings& overrides) const override;
};

NAMESPACE_SPH_END
