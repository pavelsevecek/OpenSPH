#pragma once

#include "gui/Settings.h"
#include "gui/objects/Camera.h"
#include "run/Job.h"

NAMESPACE_SPH_BEGIN

enum class GuiJobType {
    CAMERA = 3,
};

SPH_EXTEND_ENUM(GuiJobType, JobType);

class ICameraJob : public IJob {
private:
    SharedPtr<ICamera> camera;

protected:
    GuiSettings gui;

public:
    explicit ICameraJob(const std::string& name)
        : IJob(name) {}

    virtual Optional<ExtJobType> provides() const override final {
        return GuiJobType::CAMERA;
    }

    virtual JobContext getResult() const override final {
        return camera;
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

NAMESPACE_SPH_END
