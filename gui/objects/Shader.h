#pragma once

#include "gui/Settings.h"
#include "gui/objects/Palette.h"
#include "math/Curve.h"
#include "objects/containers/ArrayRef.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

class IShader : public Polymorphic {
public:
    virtual void initialize(const Storage& storage) = 0;

    virtual Rgba evaluate(const Size i) const = 0;
};

enum class RenderColorizerId {
    VELOCITY = int(ColorizerId::VELOCITY),
    ENERGY = int(QuantityId::ENERGY),
    DENSITY = int(QuantityId::DENSITY),
    DAMAGE = int(QuantityId::DAMAGE),
    GRAVITY = 666,
};


class QuantityShader : public IShader {
private:
    Curve curve;
    ColorLut lut;
    ArrayRef<const Float> data;

public:
    virtual void initialize(const Storage& storage) override;

    virtual Rgba evaluate(const Size i) const override;
};

class MaterialShader : public IShader {
private:
    Array<SharedPtr<IShader>> shaders;
    ArrayRef<const Size> matIds;

public:
    virtual Rgba evaluate(const Size i) const override {
        const IShader& shader = *shaders[matIds[i]];
        return shader.evaluate(i);
    }
};

NAMESPACE_SPH_END
