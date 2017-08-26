#pragma once

/// \file IMaterial.h
/// \brief Base class for all particle materials
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "math/rng/Rng.h"
#include "objects/containers/Map.h"
#include "objects/utility/Iterators.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

class Storage;
class IMaterial;


/// \brief Non-owning wrapper of a material and particles with this material.
///
/// This object serves as a junction between particle storage and a material. It can be used to access
/// material parameters and member function, but also provides means to iterate over particle indices in the
/// storage.
///
/// Material accessed through MaterialView shares the constness of the view, i.e. material parameters cannot
/// be modified through const MaterialView.
class MaterialView {
private:
    RawPtr<IMaterial> mat;
    IndexSequence seq;

public:
    INLINE MaterialView(RawPtr<IMaterial>&& material, IndexSequence seq)
        : mat(std::move(material))
        , seq(seq) {
        ASSERT(material != nullptr);
    }

    /// Returns the reference to the material of the particles.
    INLINE IMaterial& material() {
        ASSERT(mat != nullptr);
        return *mat;
    }

    /// Returns the const reference to the material of the particles.
    INLINE IMaterial& material() const {
        ASSERT(mat != nullptr);
        return *mat;
    }

    /// Implicit conversion to the material.
    INLINE operator IMaterial&() {
        return this->material();
    }

    /// Implicit conversion to the material.
    INLINE operator const IMaterial&() const {
        return this->material();
    }

    /// Overloaded -> operator for convenient access to material functions.
    INLINE RawPtr<IMaterial> operator->() {
        return &this->material();
    }

    /// Overloaded -> operator for convenient access to material functions.
    INLINE RawPtr<const IMaterial> operator->() const {
        return &this->material();
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


/// Shared data used when creating all bodies in the simulation
/// \todo possibly generalize, allowing to create generic context as needed by components of the run
struct MaterialInitialContext {
    /// Random number generator
    AutoPtr<IRng> rng;
};

/// \brief Material settings and functions specific for one material.
///
/// Contains all parameters needed during runtime that can differ for individual materials.
class IMaterial : public Polymorphic {
protected:
    /// Per-material parameters
    BodySettings params;

    /// Minimal values used in timestepping, do not affect values of quantities themselves.
    std::map<QuantityId, Float> minimals;

    /// Allowed range of quantities.
    std::map<QuantityId, Interval> ranges;

public:
    IMaterial(const BodySettings& settings)
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

    /// \brief Sets the timestepping parameters of given quantity.
    ///
    /// Note that the function is not thread-safe and has to be synchronized when used in parallel
    /// context.
    INLINE void setRange(const QuantityId id, const Interval& range, const Float minimal) {
        /// \todo if ranges do not contains this id, update only if the range is NOT unbounded, this way
        /// we don't add default parameters into the map and potentially speed up the getters; same for
        /// minimal
        ranges[id] = range;
        minimals[id] = minimal;
    }

    /// \brief Sets the timestepping parameters of given quantity.
    ///
    /// Syntactic sugar, using BodySettingsIds to retrieve the values from body settings of the material.
    INLINE void setRange(const QuantityId id, const BodySettingsId rangeId, const BodySettingsId minimalId) {
        ranges[id] = params.get<Interval>(rangeId);
        minimals[id] = params.get<Float>(minimalId);
    }

    /// \brief Returns the scale value of the quantity.
    ///
    /// This value is used by timestepping algorithms to determine the time step. The value can be
    /// specified by \ref setRange; if no value is specified, the fuction defaults to 0.
    INLINE Float minimal(const QuantityId id) const {
        auto iter = minimals.find(id);
        if (iter == minimals.end()) {
            return 0.f;
        }
        return iter->second;
    }

    /// \brief Returns the range of allowed quantity values.
    ///
    /// This range is enforced by timestetting algorithms, i.e. quantities do not have to be clamped by
    /// the solver or elsewhere. The range can be specified by \ref setRange; if no range is specified,
    /// the function defaults to unbounded quantity (allowing all negative and positive values).
    INLINE const Interval range(const QuantityId id) const {
        auto iter = ranges.find(id);
        if (iter == ranges.end()) {
            return Interval::unbounded();
        }
        return iter->second;
    }

    /// \brief Create all quantities needed by the material.
    virtual void create(Storage& storage, const MaterialInitialContext& context) = 0;

    /// Initialize all quantities and material parameters. Called once every step before loop.
    virtual void initialize(Storage& storage, const IndexSequence sequence) = 0;

    /// Called after derivatives are computed.
    virtual void finalize(Storage& storage, const IndexSequence sequence) = 0;
};


NAMESPACE_SPH_END
