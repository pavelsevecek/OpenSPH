#pragma once

#include "gui/Settings.h"
#include "gui/objects/Palette.h"
#include "math/Curve.h"
#include "objects/containers/ArrayRef.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

class IShader : public Polymorphic {
public:
    virtual void initialize(const Storage& storage, const RefEnum ref) = 0;

    virtual Rgba evaluateColor(const Size i) const = 0;

    virtual float evaluateScalar(const Size i) const = 0;
};

enum class RenderColorizerId {
    VELOCITY = int(ColorizerId::VELOCITY),
    ENERGY = int(QuantityId::ENERGY),
    DENSITY = int(QuantityId::DENSITY),
    DAMAGE = int(QuantityId::DAMAGE),
    GRAVITY = 666,
};


class ColorShader : public IShader {
private:
    Rgba color;
    float mult;

public:
    ColorShader(const Rgba& color, const float mult)
        : color(color)
        , mult(mult) {}

    virtual void initialize(const Storage& UNUSED(storage), const RefEnum UNUSED(ref)) override {}

    virtual Rgba evaluateColor(const Size UNUSED(i)) const override {
        return color;
    }

    virtual float evaluateScalar(const Size UNUSED(i)) const override {
        return mult;
    }
};

class QuantityShader : public IShader {
private:
    ColorLut lut;
    Curve curve;
    ArrayRef<const Float> data;

public:
    QuantityShader() = default;

    QuantityShader(const ColorLut& lut, const Curve& curve)
        : lut(lut)
        , curve(curve) {}

    virtual void initialize(const Storage& storage, const RefEnum ref) override;

    virtual Rgba evaluateColor(const Size i) const override;

    virtual float evaluateScalar(const Size i) const override;
};

class MaterialShader : public IShader {
private:
    Array<SharedPtr<IShader>> shaders;
    ArrayRef<const Size> matIds;

public:
    void addShader(const SharedPtr<IShader>& shader) {
        shaders.push(shader);
    }

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        matIds = makeArrayRef(storage.getValue<Size>(QuantityId::MATERIAL_ID), ref);
        for (const SharedPtr<IShader>& shader : shaders) {
            shader->initialize(storage, ref);
        }
    }

    virtual Rgba evaluateColor(const Size i) const override {
        const IShader& shader = *shaders[matIds[i]];
        return shader.evaluateColor(i);
    }

    virtual float evaluateScalar(const Size i) const override {
        const IShader& shader = *shaders[matIds[i]];
        return shader.evaluateScalar(i);
    }
};

NAMESPACE_SPH_END
