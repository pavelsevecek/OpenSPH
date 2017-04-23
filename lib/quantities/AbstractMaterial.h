#pragma once

#include "objects/wrappers/Iterators.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

class Storage;

namespace Abstract {
    class Material;
}

class MaterialSequence {
private:
    ArrayView<const Size> matIds;
    Size id;

    struct Comparator {
        const MaterialSequence* that;

        INLINE bool operator()(const Size matId) {
            return that->id == matId;
        }
    };

public:
    MaterialSequence(ArrayView<const Size> matIds, const Size id)
        : matIds(matIds)
        , id(id) {}

    template <typename TIdx>
    using IdxIterator = SubsetIterator<Iterator<TIdx>, Comparator>;

    IdxIterator<const Size> begin() const {
        return makeSubsetIterator(matIds.begin(), matIds.end(), Comparator{ this });
    }

    IdxIterator<const Size> end() const {
        return makeSubsetIterator(matIds.end(), matIds.end(), Comparator{ this });
    }

    operator ArrayView<const Size>() const {
        return matIds;
    }

    Size getId() const {
        return id;
    }
};

/// Non-owning wrapper of a material and particles with this material. This object serves as a junction
/// between particle storage and a material. It can be used to access material parameters and member function,
/// but also provides means to iterate over particle indices in the storage.
class MaterialView {
private:
    Abstract::Material* mat;
    ArrayView<const Size> matIdxs;
    Size id;

public:
    MaterialView(Abstract::Material* material, ArrayView<const Size> matIdxs, const Size id)
        : mat(material)
        , matIdxs(matIdxs)
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


    /// Returns iterable index sequence.
    MaterialSequence sequence() {
        return MaterialSequence(matIdxs, id);
    }

    /// Returns iterable index sequence, const version.
    const MaterialSequence sequence() const {
        return MaterialSequence(matIdxs, id);
    }
};

/// Material settings and functions specific for one material. Contains all parameters needed during runtime
/// that can differ for individual materials.
namespace Abstract {
    class Material : public Polymorphic {
    protected:
        /// per-material parameters
        BodySettings params;

        /// \todo this should probably be simple Array instead of map, faster random access with almost no
        /// overhead. Measure the difference, consider replacing with ArrayMap.

        /// minimal values used in timestepping, do not affect values of quantities themselves.
        std::map<QuantityId, Float> minimals;

        /// Allowed range of quantities.
        std::map<QuantityId, Range> ranges;

    public:
        Material(const BodySettings& settings)
            : params(settings) {}

        template <typename TValue>
        INLINE void setParam(const BodySettingsId paramIdx, TValue&& value) {
            params.set(paramIdx, std::forward<TValue>(value));
        }

        /// Returns a parameter associated with given particle.
        template <typename TValue>
        INLINE TValue getParam(const BodySettingsId paramIdx) const {
            return params.get<TValue>(paramIdx);
        }

        INLINE const BodySettings& getParams() const {
            return params;
        }

        INLINE Float& minimal(const QuantityId id) {
            return minimals[id];
        }

        INLINE Float minimal(const QuantityId id) const {
            auto iter = minimals.find(id);
            ASSERT(iter != minimals.end());
            return iter->second;
        }

        INLINE Range& range(const QuantityId id) {
            return ranges[id];
        }

        INLINE const Range& range(const QuantityId id) const {
            auto iter = ranges.find(id);
            ASSERT(iter != ranges.end());
            return iter->second;
        }

        /// Create all quantities needed by the material.
        virtual void create(Storage& storage) const = 0;

        /// Initialize all quantities and material parameters. Called once every step before loop.
        virtual void initialize(Storage& storage, const MaterialSequence sequence) = 0;

        /// Called after derivatives are computed.
        virtual void finalize(Storage& storage, const MaterialSequence sequence) = 0;
    };
}

NAMESPACE_SPH_END
