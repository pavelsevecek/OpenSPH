#include "quantities/Attractor.h"
#include "system/Settings.impl.h"

NAMESPACE_SPH_BEGIN

// clang-format off
template <>
AutoPtr<AttractorSettings> AttractorSettings::instance
    = makeAuto<AttractorSettings>(AttractorSettings{
    { AttractorSettingsId::LABEL,                 "generic.label",         ""_s,
        "String identifier of the attractor" },
    { AttractorSettingsId::BLACK_HOLE,            "black_hole.enable",     false,
        "If true, the attractor absorbs any particle that falls below the attractor's radius." },
    { AttractorSettingsId::VISUALIZATION_TEXTURE, "visualization.texture", ""_s,
        "Path to the texture image used when rendering the attractor. "},
});
// clang-format on

template class Settings<AttractorSettingsId>;
template class SettingsIterator<AttractorSettingsId>;

NAMESPACE_SPH_END
