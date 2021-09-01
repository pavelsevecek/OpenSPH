#include "gui/objects/ShaderJobs.h"
#include "gui/Factory.h"
#include "gui/windows/PaletteEditor.h"
#include "run/SpecialEntries.h"

NAMESPACE_SPH_BEGIN

const static Palette DEFAULT_PALETTE({
    { 0.f, Rgba(0.43f, 0.70f, 1.f) },
    { 0.2f, Rgba(0.5f, 0.5f, 0.5f) },
    { 0.4f, Rgba(0.65f, 0.12f, 0.01f) },
    { 0.6f, Rgba(0.79f, 0.38f, 0.02f) },
    { 0.8f, Rgba(0.93f, 0.83f, 0.34f) },
    { 1.f, Rgba(0.94f, 0.90f, 0.84f) },
});

ColorShaderJob::ColorShaderJob(const String& name)
    : IShaderJob(name) {
    color = ExtraEntry(makeAuto<ColorEntry>(Rgba::red()));
}

VirtualSettings ColorShaderJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& shaderCat = connector.addCategory("Shader parameters");
    shaderCat.connect("Color", "color", color);
    shaderCat.connect("Multiplier", "multiplier", mult);
    return connector;
}

void ColorShaderJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    ColorEntry& colorEntry = dynamic_cast<ColorEntry&>(*color.getEntry());
    result = makeShared<ColorShader>(colorEntry.getColor(), mult);
}

JobRegistrar sRegisterColorShader(
    "color shader",
    "rendering",
    [](const String& name) { return makeAuto<ColorShaderJob>(name); },
    "Shader. TODO!");


QuantityShaderJob::QuantityShaderJob(const String& name)
    : IShaderJob(name) {
    colorizerId = EnumWrapper(ShaderQuantityId::ENERGY);
    scale = EnumWrapper(PaletteScale::LINEAR);

    Curve cur(Array<CurvePoint>{ { 0.f, 1.f }, { 1.f, 1.f } });
    curve = ExtraEntry(makeAuto<CurveEntry>(cur));

    palette = ExtraEntry(makeAuto<PaletteEntry>(DEFAULT_PALETTE));
}

VirtualSettings QuantityShaderJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& shaderCat = connector.addCategory("Shader parameters");
    shaderCat.connect("Quantity", "quantity", colorizerId);
    shaderCat.connect("Lower limit", "lower_limit", lower);
    shaderCat.connect("Upper limit", "upper_limit", upper);
    shaderCat.connect("Scale", "scale", scale);
    shaderCat.connect("Palette", "palette", palette);
    shaderCat.connect("Curve", "curve", curve);
    shaderCat.connect("Multiplier", "multiplier", mult);

    return connector;
}

void QuantityShaderJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    PaletteEntry& paletteEntry = dynamic_cast<PaletteEntry&>(*palette.getEntry());
    CurveEntry& curveEntry = dynamic_cast<CurveEntry&>(*curve.getEntry());
    ColorLut lut(paletteEntry.getPalette(), Interval(lower, upper), PaletteScale(scale));
    result =
        makeShared<QuantityShader>(lut, curveEntry.getCurve().getScaled(mult), ShaderQuantityId(colorizerId));
}

JobRegistrar sRegisterShader(
    "quantity shader",
    "rendering",
    [](const String& name) { return makeAuto<QuantityShaderJob>(name); },
    "Shader. TODO!");

NAMESPACE_SPH_END
