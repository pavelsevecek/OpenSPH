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

void QuantityShader::initialize(const Storage& storage) {
    data = makeArrayRef(storage.getValue<Float>(QuantityId::ENERGY), RefEnum::WEAK);
}

Rgba QuantityShader::evaluate(const Size i) const {
    const float value = data[i];
    const Rgba color = lut(value);
    const float mult = curve(lut.paletteToRelative(value));
    return color * mult;
}

NAMESPACE_SPH_END
