#pragma once

#include "sph/av/Balsara.h"
#include "sph/av/MorrisMonaghan.h"
#include "sph/av/Riemann.h"
#include "sph/av/Standard.h"

NAMESPACE_SPH_BEGIN

/// Executes TVisitor::visit<AV>(), where AV is artificial viscosity selected in settings.
template <typename TVisitor>
INLINE decltype(auto) dispatchAV(const Settings<GlobalSettingsIds>& settings, TVisitor&& visitor) {
    const ArtificialViscosityEnum id =
        settings.get<ArtificialViscosityEnum>(GlobalSettingsIds::MODEL_AV_TYPE);
    const bool balsara = settings.get<bool>(GlobalSettingsIds::MODEL_AV_BALSARA_SWITCH);
    struct DummyAV {};
    if (balsara) {
        switch (id) {
        case ArtificialViscosityEnum::NONE:
            return visitor.template visit<DummyAV>();
        case ArtificialViscosityEnum::STANDARD:
            return visitor.template visit<BalsaraSwitch<StandardAV>>(settings);
        case ArtificialViscosityEnum::RIEMANN:
            return visitor.template visit<BalsaraSwitch<RiemannAV>>(settings);
        case ArtificialViscosityEnum::MORRIS_MONAGHAN:
            return visitor.template visit<BalsaraSwitch<MorrisMonaghanAV>>(settings);
        default:
            NOT_IMPLEMENTED;
        }
    } else {
        switch (id) {
        case ArtificialViscosityEnum::NONE:
            return visitor.template visit<DummyAV>();
        case ArtificialViscosityEnum::STANDARD:
            return visitor.template visit<StandardAV>(settings);
        case ArtificialViscosityEnum::RIEMANN:
            return visitor.template visit<RiemannAV>(settings);
        case ArtificialViscosityEnum::MORRIS_MONAGHAN:
            return visitor.template visit<MorrisMonaghanAV>(settings);
        default:
            NOT_IMPLEMENTED;
        }
    }
}

NAMESPACE_SPH_END
