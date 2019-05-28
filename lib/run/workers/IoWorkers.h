#pragma once

#include "quantities/CompressedStorage.h"
#include "run/Worker.h"

NAMESPACE_SPH_BEGIN

class LoadFileWorker : public IParticleWorker {
private:
    Path path;

public:
    LoadFileWorker(const Path& path = Path("file.ssf"))
        : IParticleWorker("")
        , path(path.native()) {}

    virtual std::string instanceName() const override {
        if (instName.empty()) {
            return "Load '" + path.fileName().native() + "'";
        } else {
            return instName;
        }
    }

    virtual std::string className() const override {
        return "load file";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return {};
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class FileSequenceWorker : public IParticleWorker {
private:
    Path firstFile;

    int maxFps = 10;

    struct {
        FlatMap<Size, CompressedStorage> data;
        bool use = false;
    } cache;

public:
    FileSequenceWorker(const std::string& name, const Path& firstFile = Path("file_0000.ssf"))
        : IParticleWorker(name)
        , firstFile(firstFile.native()) {}

    virtual std::string className() const override {
        return "load sequence";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return {};
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};


class SaveFileWorker : public IParticleWorker {
private:
    RunSettings settings;

public:
    explicit SaveFileWorker(const std::string& name);

    virtual std::string instanceName() const override {
        const Path path(settings.get<std::string>(RunSettingsId::RUN_OUTPUT_NAME));
        return "Save to '" + path.fileName().native() + "'";
    }

    virtual std::string className() const override {
        return "save file";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "particles", WorkerType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class SaveMeshWorker : public IParticleWorker {
private:
    Path path = Path("surface.ply");
    Float resolution = 1.e4_f;
    Float level = 0.13_f;
    bool scaleToUnit = false;

public:
    explicit SaveMeshWorker(const std::string& name)
        : IParticleWorker(name) {}

    virtual std::string className() const override {
        return "save mesh";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "particles", WorkerType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

NAMESPACE_SPH_END
