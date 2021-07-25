#pragma once

#include "gui/Settings.h"
#include "gui/objects/Camera.h"
#include "run/Job.h"

NAMESPACE_SPH_BEGIN

enum class GuiJobType {
    CAMERA = 3,
    IMAGE = 4,
};

SPH_EXTEND_ENUM(GuiJobType, JobType);

struct CameraData {
    AutoPtr<ICamera> camera;
    AutoPtr<ITracker> tracker;

    GuiSettings overrides = EMPTY_SETTINGS;
};

class ICameraJob : public IJob {
private:
    SharedPtr<CameraData> result;

protected:
    GuiSettings gui;

public:
    explicit ICameraJob(const std::string& name)
        : IJob(name) {}

    virtual ExtJobType provides() const override final {
        return GuiJobType::CAMERA;
    }

    virtual JobContext getResult() const override final {
        return result;
    }

    virtual UnorderedMap<std::string, ExtJobType> getSlots() const override {
        return {};
    }

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class OrthoCameraJob : public ICameraJob {
public:
    explicit OrthoCameraJob(const std::string& name);

    virtual std::string className() const override {
        return "orthographic camera";
    }

    virtual VirtualSettings getSettings() override;
};

class PerspectiveCameraJob : public ICameraJob {
public:
    explicit PerspectiveCameraJob(const std::string& name);

    virtual std::string className() const override {
        return "perspective camera";
    }

    virtual VirtualSettings getSettings() override;
};

class FisheyeCameraJob : public ICameraJob {
public:
    explicit FisheyeCameraJob(const std::string& name);

    virtual std::string className() const override {
        return "fisheye camera";
    }

    virtual VirtualSettings getSettings() override;
};

NAMESPACE_SPH_END
