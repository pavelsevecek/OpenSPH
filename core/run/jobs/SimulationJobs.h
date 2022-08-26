#pragma once

#include "run/Job.h"

NAMESPACE_SPH_BEGIN

class SphJob : public IRunJob, public SharedToken {
protected:
    RunSettings settings;
    bool isResumed = false;

public:
    explicit SphJob(const String& name, const RunSettings& overrides = EMPTY_SETTINGS);

    static RunSettings getDefaultSettings(const String& name);

    virtual String className() const override {
        return "SPH run";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES }, { "boundary", JobType::GEOMETRY } };
    }

    virtual UnorderedMap<String, ExtJobType>
    requires() const override {
        UnorderedMap<String, ExtJobType> map{ { "particles", JobType::PARTICLES } };
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

    virtual String className() const override {
        return "SPH stabilization";
    }

    virtual VirtualSettings getSettings() override;

    virtual AutoPtr<IRun> getRun(const RunSettings& overrides) const override;
};

class NBodyJob : public IRunJob, public SharedToken {
private:
    RunSettings settings;
    bool useSoft = false;
    bool isResumed = false;

public:
    explicit NBodyJob(const String& name, const RunSettings& overrides = EMPTY_SETTINGS);

    static RunSettings getDefaultSettings(const String& name);

    virtual String className() const override {
        return "N-body run";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual AutoPtr<IRun> getRun(const RunSettings& overrides) const override;
};

class PositionBasedJob : public IRunJob, public SharedToken {
protected:
    RunSettings settings;
    bool isResumed = false;

public:
    explicit PositionBasedJob(const String& name, const RunSettings& overrides = EMPTY_SETTINGS);

    virtual String className() const override {
        return "Position based run";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual AutoPtr<IRun> getRun(const RunSettings& overrides) const override;
};

NAMESPACE_SPH_END
