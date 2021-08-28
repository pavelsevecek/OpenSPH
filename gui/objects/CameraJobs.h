#pragma once

#include "gui/Settings.h"
#include "gui/objects/Camera.h"
#include "run/Job.h"

NAMESPACE_SPH_BEGIN

enum class GuiJobType {
    CAMERA = 3,
    IMAGE = 4,
    SHADER = 5,
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
    explicit ICameraJob(const String& name)
        : IJob(name) {
        gui.set(GuiSettingsId::CAMERA_POSITION, Vector(0, 0, 1.e4_f));
    }

    virtual ExtJobType provides() const override final {
        return GuiJobType::CAMERA;
    }

    virtual JobContext getResult() const override final {
        return result;
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return {};
    }

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class OrthoCameraJob : public ICameraJob {
public:
    explicit OrthoCameraJob(const String& name);

    virtual String className() const override {
        return "orthographic camera";
    }

    virtual VirtualSettings getSettings() override;
};

class PerspectiveCameraJob : public ICameraJob {
public:
    explicit PerspectiveCameraJob(const String& name);

    virtual String className() const override {
        return "perspective camera";
    }

    virtual VirtualSettings getSettings() override;
};

class FisheyeCameraJob : public ICameraJob {
public:
    explicit FisheyeCameraJob(const String& name);

    virtual String className() const override {
        return "fisheye camera";
    }

    virtual VirtualSettings getSettings() override;
};

class SphericalCameraJob : public ICameraJob {
public:
    explicit SphericalCameraJob(const String& name);

    virtual String className() const override {
        return "spherical camera";
    }

    virtual VirtualSettings getSettings() override;
};

NAMESPACE_SPH_END
