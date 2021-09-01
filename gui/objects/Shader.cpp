#include "gui/objects/Shader.h"
#include "objects/utility/EnumMap.h"

NAMESPACE_SPH_BEGIN

static RegisterEnum<ShaderQuantityId> sRenderColorizer({
    { ShaderQuantityId::VELOCITY, "velocity", "Particle velocities" },
    { ShaderQuantityId::ENERGY, "energy", "Specific internal energy" },
    { ShaderQuantityId::DENSITY, "density", "Density" },
    { ShaderQuantityId::DAMAGE, "damage", "Damage" },
    { ShaderQuantityId::GRAVITY, "gravity", "Gravitational acceleration" },
});

void QuantityShader::initialize(const Storage& storage, const RefEnum ref) {
    switch (id) {
    case ShaderQuantityId::ENERGY:
        data = makeArrayRef(storage.getValue<Float>(QuantityId::ENERGY), ref);
        break;
    case ShaderQuantityId::VELOCITY: {
        ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
        Array<Float> values(v.size());
        for (Size i = 0; i < v.size(); ++i) {
            values[i] = getLength(v[i]);
        }
        data = makeArrayRef(std::move(values), ref);
        break;
    }
    case ShaderQuantityId::DENSITY:
        data = makeArrayRef(storage.getValue<Float>(QuantityId::DENSITY), ref);
        break;
    case ShaderQuantityId::DAMAGE:
        data = makeArrayRef(storage.getValue<Float>(QuantityId::DAMAGE), ref);
        break;
    default:
        NOT_IMPLEMENTED;
    }
}

Rgba QuantityShader::evaluateColor(const Size i) const {
    const float value = data[i];
    return lut(value);
}

float QuantityShader::evaluateScalar(const Size i) const {
    const float value = data[i];
    return curve(lut.paletteToRelative(value));
}

NAMESPACE_SPH_END
