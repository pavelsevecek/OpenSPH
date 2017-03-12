#pragma once

#include "objects/wrappers/Iterators.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

class Storage;

namespace Abstract {
    class Material;
}

/// Non-owning view of particles having the same material, providing means to iterate over particle indices in
/// storage.
class MaterialSequence {
private:
    Abstract::Material* material;
    ArrayView<Size> matIds;
    Size id;

public:
    MaterialSequence(Abstract::Material* material, ArrayView<Size> matIds, const Size id)
        : material(material)
        , matIds(matIds)
        , id(id) {
        ASSERT(material != nullptr);
    }

    Abstract::Material& material() {
        return material;
    }

    auto begin() {
        return makeSubsetIterator(
            matIds.begin(), matIds.end(), [id](const Size matId) { return id == matId; });
    }

    auto end() {
        return makeSubsetIterator(matIds.end(), matIds.end(), [id](const Size matId) { return id == matId; });
    }
};

/// Material settings and functions specific for one material. Contains all parameters needed during runtime
/// that can differ for individual materials.
namespace Abstract {
    class Material : public Polymorphic {
    private:
        /// per-material parameters
        BodySettings params;

        /// minimal values used in timestepping, do not affect values of quantities themselves.
        std::map<QuantityIds, Float> minimals;

        /// Allowed range of quantities.
        std::map<QuantityIds, Range> ranges;

    public:
        void setParams(const BodySettingsIds paramIdx, const BodySettings& settings) {
            settings.copyValueTo(paramIdx, params);
        }

        template <typename TValue>
        void setParams(const BodySettingsIds paramIdx, TValue&& value) {
            params.set(paramIdx, value); // cannot move here if we have >1 material
        }

        /// Returns a parameter associated with given particle.
        template <typename TValue>
        TValue getParam(const BodySettingsIds paramIdx) const {
            return params.get<TValue>(paramIdx);
        }

        Float& minimal(const QuantityIds id) {
            minimals[id];
        }

        Range& range(const QuantityIds id) {
            ranges[id];
        }

        /// Initialize all quantities and material parameters. Called once every step before loop.
        virtual void initialize(Storage& storage, const MaterialSequence sequence) = 0;

        /// Called after derivatives are computed.
        virtual void finalize(Storage& storage, const MaterialSequence sequence) = 0;
    };
}

NAMESPACE_SPH_END
