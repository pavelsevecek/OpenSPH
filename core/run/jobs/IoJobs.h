#pragma once

#include "run/Job.h"

NAMESPACE_SPH_BEGIN

class Triangle;

enum class UnitEnum {
    SI,
    CGS,
    NBODY,
};

Float getGravityConstant(const UnitEnum units);

class LoadFileJob : public IParticleJob {
private:
    Path path;
    EnumWrapper units = EnumWrapper(UnitEnum::SI);

public:
    LoadFileJob(const Path& path = Path("file.ssf"))
        : IParticleJob("")
        , path(path) {}

    virtual String instanceName() const override {
        if (instName.empty()) {
            return "Load '" + path.fileName().string() + "'";
        } else {
            return instName;
        }
    }

    virtual String className() const override {
        return "load file";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
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
    FileSequenceJob(const String& name, const Path& firstFile = Path("file_0000.ssf"))
        : IParticleJob(name)
        , firstFile(firstFile) {}

    virtual String className() const override {
        return "load sequence";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return {};
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class SaveFileJob : public IParticleJob {
private:
    RunSettings settings;

public:
    explicit SaveFileJob(const String& name);

    virtual String instanceName() const override {
        if (instName.empty()) {
            const Path path(settings.get<String>(RunSettingsId::RUN_OUTPUT_NAME));
            return "Save to '" + path.fileName().string() + "'";
        } else {
            return instName;
        }
    }

    virtual String className() const override {
        return "save file";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

enum class MeshAlgorithm {
    MARCHING_CUBES,
    ALPHA_SHAPE,
};

class SaveMeshJob : public IParticleJob {
private:
    Path path = Path("surface.ply");
    Float resolution = 0.5_f;

    EnumWrapper algorithm = EnumWrapper(MeshAlgorithm::MARCHING_CUBES);

    Float level = 0.13_f;
    Float smoothingMult = 1._f;
    bool anisotropic = false;

    Float alpha = 4._f;

    bool scaleToUnit = false;
    bool refine = false;

public:
    explicit SaveMeshJob(const String& name)
        : IParticleJob(name) {}

    virtual String className() const override {
        return "save mesh";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;

private:
    Array<Triangle> runMarchingCubes(const Storage& storage,
        const RunSettings& global,
        IRunCallbacks& callbacks) const;

    Array<Triangle> runAlphaShape(const Storage& storage, IRunCallbacks& callbacks) const;
};

NAMESPACE_SPH_END
