#include "gui/objects/Shader.h"
#include "objects/utility/EnumMap.h"

NAMESPACE_SPH_BEGIN

static RegisterEnum<RenderColorizerId> sRenderColorizer({
    { RenderColorizerId::VELOCITY, "velocity", "Particle velocities" },
    { RenderColorizerId::ENERGY, "energy", "Specific internal energy" },
    { RenderColorizerId::DENSITY, "density", "Density" },
    { RenderColorizerId::DAMAGE, "damage", "Damage" },
    { RenderColorizerId::GRAVITY, "gravity", "Gravitational acceleration" },
});

void QuantityShader::initialize(const Storage& storage, const RefEnum ref) {
    data = makeArrayRef(storage.getValue<Float>(QuantityId::ENERGY), ref);
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
