#include "quantities/Attractor.h"
#include "quantities/Storage.h"
#include "system/Settings.impl.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

static RegisterEnum<ParticleInteractionEnum> sInteraction({
    { ParticleInteractionEnum::NONE, "none", "No interaction" },
    { ParticleInteractionEnum::ABSORB, "absorb", "Particles falling into the attractor are absorbed" },
    { ParticleInteractionEnum::REPEL, "repel", "Particles are repelled" },
});

// clang-format off
template <>
const AttractorSettings& getDefaultSettings() {
    static AttractorSettings instance({
    { AttractorSettingsId::LABEL,                 "generic.label",         ""_s,
        "String identifier of the attractor" },
    { AttractorSettingsId::INTERACTION,           "interactions",     ParticleInteractionEnum::NONE,
        "Specifies how the attractor interacts with particles. Can be one of:\n" +
        EnumMap::getDesc<ParticleInteractionEnum>() },
    { AttractorSettingsId::VISIBLE,               "visualization.visible", true,
        "Visible when rendered. "},
    { AttractorSettingsId::VISUALIZATION_TEXTURE, "visualization.texture", ""_s,
        "Path to the texture image used when rendering the attractor." },
    { AttractorSettingsId::ALBEDO,                "visualization.albedo", 1._f,
        "Albedo of the object." },
    });
    return instance;
}
// clang-format on

template class Settings<AttractorSettingsId>;
template class SettingsIterator<AttractorSettingsId>;

void Attractor::interact(IScheduler& scheduler, Storage& storage) {
    const ParticleInteractionEnum type = settings.getOr<ParticleInteractionEnum>(
        AttractorSettingsId::INTERACTION, ParticleInteractionEnum::NONE);
    switch (type) {
    case ParticleInteractionEnum::NONE:
        break;
    case ParticleInteractionEnum::ABSORB: {
        /// \todo parallelize
        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        Array<Size> toRemove;
        for (Size i = 0; i < r.size(); ++i) {
            if (getSqrLength(position - r[i]) < sqr(radius)) {
                toRemove.push(i);
            }
        }
        storage.remove(toRemove, Storage::IndicesFlag::INDICES_SORTED | Storage::IndicesFlag::PROPAGATE);
        break;
    }
    case ParticleInteractionEnum::REPEL: {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
        parallelFor(scheduler, 0, r.size(), [&](const Size i) {
            if (getSqrLength(position - r[i]) < sqr(radius)) {
                const Vector dir = getNormalized(r[i] - position);
                r[i] = setH(position + dir * radius, r[i][H]);
                const Float v_par = dot(v[i] - velocity, dir);
                if (v_par < 0) {
                    // flies towards the attractor, remove the parallel component of velocity
                    v[i] = clearH(v[i] - v_par * dir);
                }
            }
        });
        break;
    }
    default:
        NOT_IMPLEMENTED;
    }
}

NAMESPACE_SPH_END
