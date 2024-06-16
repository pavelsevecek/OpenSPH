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
    { AttractorSettingsId::SPRING_CONSTANT,       "spring_constant",  0.004_f,
        "Constant determining the softness of the collision." },
    { AttractorSettingsId::EPSILON,               "epsilon",          0.5_f,
        "Constant determining how inelastic the collision is." },
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

inline Float orbitTime(Float mass, Float a, Float G = Constants::gravity) {
    const Float rhs = (G * mass) / (4 * sqr(PI));
    return sqrt(pow<3>(a) / rhs);
}

void Attractor::interact(IScheduler& scheduler, Storage& storage, const Float dt) {
    const ParticleInteractionEnum type = settings.getOr<ParticleInteractionEnum>(
        AttractorSettingsId::INTERACTION, ParticleInteractionEnum::NONE);
    switch (type) {
    case ParticleInteractionEnum::NONE:
        break;
    case ParticleInteractionEnum::ABSORB: {
        /// \todo parallelize
        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        ArrayView<const Float> rho;
        if (storage.has(QuantityId::DENSITY)) {
            rho = storage.getValue<Float>(QuantityId::DENSITY);
        }
        Array<Size> toRemove;
        Float absorbed_mass = 0._f;
        Float absorbed_volume = 0._f;
        Vector absorbed_momentum = Vector(0);
        for (Size i = 0; i < r.size(); ++i) {
            if (getSqrLength(position - r[i]) < sqr(radius)) {
                toRemove.push(i);
                absorbed_mass += m[i];
                absorbed_volume += !rho.empty() ? m[i] / rho[i] : 0;
                absorbed_momentum += m[i] * (v[i] - velocity);
            }
        }
        if (absorbed_mass > 0) {
            velocity += absorbed_momentum / mass;
            mass += absorbed_mass;
            radius = volumeEquivalentRadius(sphereVolume(radius) + absorbed_volume);
        }
        storage.remove(toRemove, Storage::IndicesFlag::INDICES_SORTED | Storage::IndicesFlag::PROPAGATE);
        break;
    }
    case ParticleInteractionEnum::REPEL: {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        const Float springConstant = settings.getOr<Float>(AttractorSettingsId::SPRING_CONSTANT, 0.004_f);
        const Float epsilon = settings.getOr<Float>(AttractorSettingsId::EPSILON, 0.5_f);
        constexpr Float h1 = sqr(PI);
        const Float h2 = 2 * PI / sqrt(sqr(PI / log(epsilon)) + 1);
        parallelFor(scheduler, 0, r.size(), [&](const Size i) {
            if (getSqrLength(position - r[i]) < sqr(radius + r[i][H])) {
                Vector dir;
                Float dist;
                tieToTuple(dir, dist) = getNormalizedWithLength(r[i] - position);
                const Float alpha = r[i][H] + radius - dist;
                SPH_ASSERT(alpha >= 0);
                const Vector delta_v = v[i] - velocity;
                const Float alpha_dot = -dot(delta_v, dir);
                const Float m_eff = (m[i] * mass) / (m[i] + mass);
                const Float t_dur = springConstant * orbitTime(m[i] + mass, r[i][H] + radius);
                const Float k1 = m_eff * h1 / sqr(t_dur);
                const Float k2 = m_eff * h2 / t_dur;
                const Vector force = (k1 * alpha + k2 * alpha_dot) * dir;
                acceleration -= force / mass;
                v[i] += clearH(force / m[i] * dt);
            }
        });
        break;
    }
    default:
        NOT_IMPLEMENTED;
    }
}

NAMESPACE_SPH_END
