#include "gui/objects/ShaderJobs.h"
#include "gui/Factory.h"
#include "gui/windows/PaletteEditor.h"
#include "run/SpecialEntries.h"

NAMESPACE_SPH_BEGIN

ShaderJob::ShaderJob(const String& name)
    : IJob(name) {
    colorizerId = EnumWrapper(RenderColorizerId::ENERGY);
    scale = EnumWrapper(PaletteScale::LINEAR);
    curve = ExtraEntry(makeAuto<CurveEntry>());
    palette = ExtraEntry(makeAuto<PaletteEntry>(Factory::getPalette(QuantityId::ENERGY)));
}

VirtualSettings ShaderJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& shaderCat = connector.addCategory("Shader parameters");
    shaderCat.connect("Quantity", "quantity", colorizerId);
    shaderCat.connect("Lower limit", "lower_limit", lower);
    shaderCat.connect("Upper limit", "upper_limit", upper);
    shaderCat.connect("Scale", "scale", scale);
    shaderCat.connect("Palette", "palette", palette);
    shaderCat.connect("Curve", "curve", curve);

    return connector;
}

void ShaderJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    result = makeShared<QuantityShader>();
}

JobRegistrar sRegisterShader(
    "shader",
    "rendering",
    [](const String& name) { return makeAuto<ShaderJob>(name); },
    "Shader. TODO!");


NAMESPACE_SPH_END
