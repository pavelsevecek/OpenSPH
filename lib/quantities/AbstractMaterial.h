#pragma once

#include "objects/wrappers/Iterators.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

class Storage;

namespace Abstract {
    class Material;
}

/// Non-owning wrapper of a material and particles with this material. This object serves as a junction
/// between particle storage and a material. It can be used to access material parameters and member function,
/// but also provides means to iterate over particle indices in the storage.
class MaterialSequence {
private:
    Abstract::Material* mat;
    ArrayView<Size> matIds;
    Size id;

    struct Comparator {
        const MaterialSequence* that;

        INLINE bool operator()(const Size matId) {
            return that->id == matId;
        }
    };

public:
    MaterialSequence(Abstract::Material* material, ArrayView<Size> matIds, const Size id)
        : mat(material)
        , matIds(matIds)
        , id(id) {
        ASSERT(material != nullptr);
    }

    /// Returns reference to the material of the particles.
    Abstract::Material& material() {
        ASSERT(mat != nullptr);
        return *mat;
    }

    /// Implicit conversion to the material.
    operator Abstract::Material&() {
        ASSERT(mat != nullptr);
        return *mat;
    }

    /// Overloaded -> operator for convenient access to material functions.
    Abstract::Material* operator->() {
        return mat;
    }

    /// \copydoc Abstract::Material* operator->()
    const Abstract::Material* operator->() const {
        return mat;
    }

    template <typename TIdx>
    using IdxIterator = SubsetIterator<Iterator<TIdx>, Comparator>;

    IdxIterator<Size> begin() {
        return makeSubsetIterator(matIds.begin(), matIds.end(), Comparator{ this });
    }

    IdxIterator<const Size> begin() const {
        return makeSubsetIterator(matIds.begin(), matIds.end(), Comparator{ this });
    }

    IdxIterator<Size> end() {
        return makeSubsetIterator(matIds.end(), matIds.end(), Comparator{ this });
    }

    IdxIterator<const Size> end() const {
        return makeSubsetIterator(matIds.end(), matIds.end(), Comparator{ this });
    }

    /// Creates an array of indices corresponding to particles with this material. This is an alternative to
    /// directly using begin() and end() iterators, can be handy in performance-critical code where one needs
    /// to iterate over indices many times.
    /*Array<Size> cache() {
        Array<Size> cached;
        for (Size id : *this) {
            cached.push(id);
        }
        return cached;
    }*/
};

/// Material settings and functions specific for one material. Contains all parameters needed during runtime
/// that can differ for individual materials.
namespace Abstract {
    class Material : public Polymorphic {
    private:
        /// per-material parameters
        BodySettings params;

        /// \todo this should probably be simple Array instead of map, faster random access with almost no
        /// overhead. Measure the difference, consider replacing with ArrayMap.

        /// minimal values used in timestepping, do not affect values of quantities themselves.
        std::map<QuantityIds, Float> minimals;

        /// Allowed range of quantities.
        std::map<QuantityIds, Range> ranges;

    public:
        INLINE void setParams(const BodySettingsIds paramIdx, const BodySettings& settings) {
            settings.copyValueTo(paramIdx, params);
        }

        template <typename TValue>
        INLINE void setParams(const BodySettingsIds paramIdx, TValue&& value) {
            params.set(paramIdx, value); // cannot move here if we have >1 material
        }

        /// Returns a parameter associated with given particle.
        template <typename TValue>
        INLINE TValue getParam(const BodySettingsIds paramIdx) const {
            return params.get<TValue>(paramIdx);
        }

        INLINE Float& minimal(const QuantityIds id) {
            return minimals[id];
        }

        INLINE Float minimal(const QuantityIds id) const {
            auto iter = minimals.find(id);
            ASSERT(iter != minimals.end());
            return iter->second;
        }

        INLINE Range& range(const QuantityIds id) {
            return ranges[id];
        }

        INLINE const Range& range(const QuantityIds id) const {
            auto iter = ranges.find(id);
            ASSERT(iter != ranges.end());
            return iter->second;
        }

        /// Initialize all quantities and material parameters. Called once every step before loop.
        virtual void initialize(Storage& storage, const MaterialSequence sequence) = 0;

        /// Called after derivatives are computed.
        virtual void finalize(Storage& storage, const MaterialSequence sequence) = 0;
    };
}

NAMESPACE_SPH_END
