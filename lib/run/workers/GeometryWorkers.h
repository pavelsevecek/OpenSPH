#pragma once

#include "run/Worker.h"

NAMESPACE_SPH_BEGIN

class SphereWorker : public IGeometryWorker {
private:
    Float radius = 1.e5_f;

public:
    explicit SphereWorker(const std::string& name);

    virtual std::string className() const override;

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override;

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class BlockWorker : public IGeometryWorker {
private:
    Vector center = Vector(0._f);
    Vector dimensions = Vector(1.e5_f);

public:
    explicit BlockWorker(const std::string& name)
        : IGeometryWorker(name) {}

    virtual std::string className() const override {
        return "block";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return {};
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class EllipsoidWorker : public IGeometryWorker {
private:
    Vector semiaxes = Vector(2.e5_f, 1.e5_f, 1.e5_f);

public:
    explicit EllipsoidWorker(const std::string& name)
        : IGeometryWorker(name) {}

    virtual std::string className() const override {
        return "ellipsoid";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return {};
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class CylinderWorker : public IGeometryWorker {
private:
    Float radius = 1.e5_f;
    Float height = 2.e5_f;

public:
    explicit CylinderWorker(const std::string& name);

    virtual std::string className() const override;

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override;

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class HalfSpaceWorker : public IGeometryWorker {
public:
    explicit HalfSpaceWorker(const std::string& name)
        : IGeometryWorker(name) {}

    virtual std::string className() const override {
        return "half space";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return {};
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class MeshGeometryWorker : public IGeometryWorker {
private:
    Path path = Path("file.ply");
    Float scale = 1._f;

public:
    explicit MeshGeometryWorker(const std::string& name);

    virtual std::string className() const override;

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override;

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class ParticleGeometryWorker : public IGeometryWorker {
private:
    Float resolution = 1.e3_f;
    Float surfaceLevel = 0.15_f;

public:
    explicit ParticleGeometryWorker(const std::string& name)
        : IGeometryWorker(name) {}

    virtual std::string className() const override {
        return "particle geometry";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "particles", WorkerType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class SpheresGeometryWorker : public IGeometryWorker {
public:
    explicit SpheresGeometryWorker(const std::string& name)
        : IGeometryWorker(name) {}

    virtual std::string className() const override {
        return "spheres geometry";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "spheres", WorkerType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class InvertGeometryWorker : public IGeometryWorker {
public:
    explicit InvertGeometryWorker(const std::string& name)
        : IGeometryWorker(name) {}

    virtual std::string className() const override {
        return "invert geometry";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "geometry", WorkerType::GEOMETRY } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class TransformGeometryWorker : public IGeometryWorker {
private:
    Vector scaling = Vector(1._f);
    Vector offset = Vector(0._f);

public:
    explicit TransformGeometryWorker(const std::string& name)
        : IGeometryWorker(name) {}

    virtual std::string className() const override {
        return "transform geometry";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "geometry", WorkerType::GEOMETRY } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

enum class BooleanEnum {
    UNION,
    DIFFERENCE,
    INTERSECTION,
};
static RegisterEnum<BooleanEnum> sBoolean({
    { BooleanEnum::UNION, "union", "union" },
    { BooleanEnum::INTERSECTION, "intersection", "intersection" },
    { BooleanEnum::DIFFERENCE, "difference", "difference" },
});


class BooleanGeometryWorker : public IGeometryWorker {
    EnumWrapper mode;
    Vector offset = Vector(0._f);

public:
    BooleanGeometryWorker(const std::string& name)
        : IGeometryWorker(name) {
        mode = EnumWrapper(BooleanEnum::DIFFERENCE);
    }

    virtual std::string className() const override {
        return "boolean";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "operand A", WorkerType::GEOMETRY }, { "operand B", WorkerType::GEOMETRY } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

NAMESPACE_SPH_END
