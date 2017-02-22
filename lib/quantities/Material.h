#pragma once

#include "objects/wrappers/Range.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Eos;
}
class Storage;

/// Material settings and functions specific for one material. Contains all parameters needed during runtime
/// that can differ for individual materials.
struct Material : public Noncopyable {
    BodySettings params;

    /// \todo this is very problem-specific, we don't need any eos when only gravity is considered, for
    /// example ...
    std::unique_ptr<Abstract::Eos> eos;

    /// Minimal values used in timestepping. Do not affect values of quantities themselves.
    std::map<QuantityIds, Float> minimals;

    Material();

    Material(Material&& other) = default;

    ~Material();

    Material& operator=(Material&& other) = default;
};

/// Object providing access to material parameters of individual particles.
class MaterialAccessor {
private:
    ArrayView<Size> matIdxs;
    ArrayView<Material> materials;

public:
    MaterialAccessor(Storage& storage);

    /// Sets a parameter associated for all particles by copying value from settings.
    void setParams(const BodySettingsIds paramIdx, const BodySettings& settings);

    template <typename TValue>
    void setParams(const BodySettingsIds paramIdx, const TValue& value) {
        for (Material& mat : materials) {
            mat.params.set(paramIdx, value); // cannot move here if we have >1 material
        }
    }

    Float& minimal(const QuantityIds id, const Size particleIdx) {
        Material& mat = materials[matIdxs[particleIdx]];
        return mat.minimals[id];
    }

    /// Returns a parameter associated with given particle.
    /// \todo values are saved in variant, so there is no overhead in calling Variant::get<TValue>, however
    /// the parameter lookup in map can be potentially expensive (although it is O(log N)), we can potentially
    /// cache iterators for fast access to parameters. We usually want to access only one or two material
    /// parameters anyway.
    template <typename TValue>
    TValue getParam(const BodySettingsIds paramIdx, const Size particleIdx) const {
        const Material& mat = materials[matIdxs[particleIdx]];
        return mat.params.get<TValue>(paramIdx);
    }
};


/// Object providing accessor to equation of state individual particles.
class EosAccessor {
private:
    ArrayView<Size> matIdxs;
    ArrayView<Material> materials;
    ArrayView<Float> rho, u;

public:
    EosAccessor(Storage& storage);

    /// Returns a pressure and sound speed from an equation of state for given particle.
    Tuple<Float, Float> evaluate(const Size particleIdx) const;
};

NAMESPACE_SPH_END
