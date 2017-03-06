#pragma once

#include "objects/wrappers/Iterators.h"
#include "system/Settings.h"


NAMESPACE_SPH_BEGIN

class Storage;

// name?
/// View of given material, can iterate over particles of given material id
class MaterialView {
private:
    ArrayView<Size> matIds;
    Size id;

public:
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
    struct Material : public Polymorphic {
        /// Creating storage:
        /// Storage(std::unique_ptr<Abstract::MAterial>&& mat) {

        Size id; /// MatId of this material in the storage. One material can only belong to one storage.

        /// per-material parameters
        BodySettings params;

        /// minimal values used in timestepping, do not affect values of quantities themselves.
        std::map<QuantityIds, Float> minimals;


        /// Initialize all quantities and material parameters. Called once every step before loop.
        virtual void initialize(Sequence, Storage& storage) = 0;

        /// Called after derivatives are computed.
        virtual void finalize(Storage& storage) = 0;

        /// Returns values of quantity from the material. If the material does not affect the quantity in any
        /// way, simply returns the quantity as stored in storage. Can only be called between calls of \ref
        /// initialize and \ref finalize each step.
        /// \todo move modifier here
        virtual Quantity& getValue(const QuantityIds& key) = 0;
    };
}

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


NAMESPACE_SPH_END
