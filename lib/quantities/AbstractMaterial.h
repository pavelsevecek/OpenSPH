#pragma once

/// \file AbstractMaterial.h
/// \brief Base class for all particle materials
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

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
class MaterialView {
private:
    Abstract::Material* mat;
    IndexSequence seq;

public:
    INLINE MaterialView(Abstract::Material* material, IndexSequence seq)
        : mat(material)
        , seq(seq) {
        ASSERT(material != nullptr);
    }

    /// Returns reference to the material of the particles.
    INLINE Abstract::Material& material() {
        ASSERT(mat != nullptr);
        return *mat;
    }

    /// Implicit conversion to the material.
    INLINE operator Abstract::Material&() {
        ASSERT(mat != nullptr);
        return *mat;
    }

    /// Overloaded -> operator for convenient access to material functions.
    INLINE Abstract::Material* operator->() {
        return mat;
    }

    /// \copydoc Abstract::Material* operator->()
    INLINE const Abstract::Material* operator->() const {
        return mat;
    }


    /// Returns iterable index sequence.
    INLINE IndexSequence sequence() {
        return seq;
    }

    /// Returns iterable index sequence, const version.
    INLINE const IndexSequence sequence() const {
        return seq;
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

        /// Returns settings containing material parameters
        INLINE const BodySettings& getParams() const {
            return params;
        }

        /// Returns a reference value for given quantity
        INLINE Float& minimal(const QuantityId id) {
            return minimals[id];
        }

        /// \copydoc Float& minimal(const QuantityId id)
        INLINE Float minimal(const QuantityId id) const {
            auto iter = minimals.find(id);
            ASSERT(iter != minimals.end());
            return iter->second;
        }

        /// Returns range of allowed values for given quantity
        INLINE Range& range(const QuantityId id) {
            return ranges[id];
        }

        /// \copydoc Range& range(const QuantityId id)
        INLINE const Range& range(const QuantityId id) const {
            auto iter = ranges.find(id);
            ASSERT(iter != ranges.end());
            return iter->second;
        }

        /// Create all quantities needed by the material.
        virtual void create(Storage& storage) const = 0;

        /// Initialize all quantities and material parameters. Called once every step before loop.
        virtual void initialize(Storage& storage, const IndexSequence sequence) = 0;

        /// Called after derivatives are computed.
        virtual void finalize(Storage& storage, const IndexSequence sequence) = 0;
    };
}

NAMESPACE_SPH_END
