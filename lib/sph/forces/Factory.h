#pragma once

#include "sph/av/Factory.h"
#include "sph/forces/Damage.h"
#include "sph/forces/StressForce.h"
#include "sph/forces/Yielding.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

/// Executes TVisitor::visit<Yielding>() with yielding object selected from settings.
template <typename TVisitor, typename... TArgs>
INLINE decltype(auto) dispatchYielding(const GlobalSettings& settings, TVisitor&& visitor, TArgs&&... args) {
    const YieldingEnum id = settings.get<YieldingEnum>(GlobalSettingsIds::MODEL_YIELDING);
    switch (id) {
    case YieldingEnum::NONE:
        return visitor.template visit<DummyYielding>(settings, std::forward<TArgs>(args)...);
    case YieldingEnum::VON_MISES:
        return visitor.template visit<VonMisesYielding>(settings, std::forward<TArgs>(args)...);
    default:
        NOT_IMPLEMENTED;
    }
}

/// Executes TVisitor::visit<Damage>() with damage object selected from settings.
template <typename TVisitor, typename... TArgs>
INLINE decltype(auto) dispatchDamage(const GlobalSettings& settings, TVisitor&& visitor, TArgs&&... args) {
    const DamageEnum id = settings.get<DamageEnum>(GlobalSettingsIds::MODEL_DAMAGE);
    switch (id) {
    case DamageEnum::NONE:
        return visitor.template visit<DummyDamage>(settings, std::forward<TArgs>(args)...);
    case DamageEnum::SCALAR_GRADY_KIPP:
        return visitor.template visit<ScalarDamage>(settings, std::forward<TArgs>(args)...);
    default:
        NOT_IMPLEMENTED;
    }
}


/// Build up stress force from settings

template <typename Yielding, typename AV, typename TForceVisitor>
struct DamageVisitor {
    TForceVisitor&& visitor;

    template <typename Damage>
    decltype(auto) visit(const GlobalSettings& settings) {
        return visitor.template visit<StressForce<Yielding, Damage, AV>>(settings);
    }
};

template <typename AV, typename TForceVisitor>
struct YieldingVisitor {
    TForceVisitor&& visitor;

    template <typename Yielding>
    decltype(auto) visit(const GlobalSettings& settings) {
        DamageVisitor<Yielding, AV, TForceVisitor&&> damageVisitor{ std::forward<TForceVisitor>(visitor) };
        return dispatchDamage(settings, std::move(damageVisitor));
    }
};

template <typename TForceVisitor>
struct AvVisitor {
    TForceVisitor&& visitor;

    template <typename AV>
    decltype(auto) visit(const GlobalSettings& settings) {
        YieldingVisitor<AV, TForceVisitor&&> yieldingVisitor{ std::forward<TForceVisitor>(visitor) };
        return dispatchYielding(settings, yieldingVisitor);
    }
};

template <typename TVisitor>
INLINE decltype(auto) dispatchStressForce(const GlobalSettings& settings, TVisitor&& visitor) {
    return dispatchAV(settings, AvVisitor<TVisitor&&>{ std::forward<TVisitor>(visitor) });
}

NAMESPACE_SPH_END
