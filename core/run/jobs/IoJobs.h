#pragma once

#include "quantities/CompressedStorage.h"
#include "run/Job.h"

NAMESPACE_SPH_BEGIN

class LoadFileJob : public IParticleJob {
private:
    Path path;

public:
    LoadFileJob(const Path& path = Path("file.ssf"))
        : IParticleJob("")
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

    virtual UnorderedMap<std::string, ExtJobType> getSlots() const override {
        return {};
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

FlatMap<Size, Path> getFileSequence(const Path& firstFile);

class FileSequenceJob : public IParticleJob {
private:
    Path firstFile;

    int maxFps = 10;

public:
    FileSequenceJob(const std::string& name, const Path& firstFile = Path("file_0000.ssf"))
        : IParticleJob(name)
        , firstFile(firstFile.native()) {}

    virtual std::string className() const override {
        return "load sequence";
    }

    virtual UnorderedMap<std::string, ExtJobType> getSlots() const override {
        return {};
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class SaveFileJob : public IParticleJob {
private:
    RunSettings settings;

public:
    explicit SaveFileJob(const std::string& name);

    virtual std::string instanceName() const override {
        if (instName.empty()) {
            const Path path(settings.get<std::string>(RunSettingsId::RUN_OUTPUT_NAME));
            return "Save to '" + path.fileName().native() + "'";
        } else {
            return instName;
        }
    }

    virtual std::string className() const override {
        return "save file";
    }

    virtual UnorderedMap<std::string, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class SaveMeshJob : public IParticleJob {
private:
    Path path = Path("surface.ply");
    Float resolution = 1.e4_f;
    Float level = 0.13_f;
    Float smoothingMult = 1._f;
    bool anisotropic = false;
    bool scaleToUnit = false;
    bool refine = false;

public:
    explicit SaveMeshJob(const std::string& name)
        : IParticleJob(name) {}

    virtual std::string className() const override {
        return "save mesh";
    }

    virtual UnorderedMap<std::string, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

NAMESPACE_SPH_END
