#pragma once

/// \file Integrals.h
/// \brief Integrals of motion and other integral quantities
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "common/ForwardDecl.h"
#include "math/Means.h"
#include "objects/containers/Array.h"
#include "objects/utility/Dynamic.h"
#include "objects/wrappers/Function.h"
#include "quantities/QuantityIds.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

/// \brief Interface for classes computing integral quantities from storage
///
/// This interface is used to get reduced information from all particles (and possibly all quantities) in the
/// storage. The result is a single value, type of which is given by the template parameter. This is useful to
/// get integrals of motion, as the name suggests, as well as other useful values, like
/// average/minimal/maximal value of given quantity, etc.
///
/// \todo automatically exclude ghost particles?
template <typename Type>
class IIntegral : public Polymorphic {
public:
    /// \brief Computes the integral quantity using particles in the storage.
    ///
    /// Storage must contain quantites relevant to the integral implementation. Generally positions, masses
    /// and density must be present.
    virtual Type evaluate(const Storage& storage) const = 0;

    /// \brief Returns the name of the integral.
    ///
    /// Needed to label the integral in logs, GUI etc.
    virtual String getName() const = 0;
};


/// \brief Computes the total mass of all SPH particles.
///
/// Storage must contains particle masses, of course; checked by assert.
///
/// \note Total mass is always conserved automatically as particles do not change their mass. This is
/// therefore only useful as a sanity check, or potentially when using solver with variable number of
/// particles.
class TotalMass : public IIntegral<Float> {
public:
    virtual Float evaluate(const Storage& storage) const override;

    virtual String getName() const override {
        return "Total mass";
    }
};

/// \brief Computes total momentum of all SPH particles with a respect to the reference frame.
///
/// Storage must contain at least particle masses and particle positions with velocities, checked by
/// assert.
class TotalMomentum : public IIntegral<Vector> {
private:
    Vector omega;

public:
    explicit TotalMomentum(const Float omega = 0._f);

    virtual Vector evaluate(const Storage& storage) const override;

    virtual String getName() const override {
        return "Total momentum";
    }
};

/// \brief Computes total angular momentum of all SPH particles with a respect to the reference frame.
///
/// Storage must contain at least particle masses and particle positions with velocities, checked by
/// assert. If the particles also have nonzero angular velocities, the 'local' angular momenta are added to
/// the total angular momentum.
class TotalAngularMomentum : public IIntegral<Vector> {
private:
    Vector frameFrequency;

public:
    explicit TotalAngularMomentum(const Float frameFrequency = 0._f);

    virtual Vector evaluate(const Storage& storage) const override;

    virtual String getName() const override {
        return "Total angular momentum";
    }
};

/// \brief Returns the total kinetic energy of all particles.
///
/// Storage must contain at least particle masses and particle positions with velocities, checked by assert.
/// If the particles also have nonzero angular velocities, the rotational energy is added to the total kinetic
/// energy.
class TotalKineticEnergy : public IIntegral<Float> {
private:
    Vector omega;

public:
    explicit TotalKineticEnergy(const Float omega = 0._f);

    virtual Float evaluate(const Storage& storage) const override;

    virtual String getName() const override {
        return "Kinetic energy";
    }
};

/// \brief Returns the total internal energy of all particles.
///
/// Storage must contain at least particle masses and specific internal energy. If used solver works with
/// other independent quantity (energy density, total energy, specific entropy), specific energy must be
/// derived and saved to storage before the function is called.
class TotalInternalEnergy : public IIntegral<Float> {
public:
    virtual Float evaluate(const Storage& storage) const override;

    virtual String getName() const override {
        return "Internal energy";
    }
};

/// \brief Returns the total energy of all particles.
///
/// This is simply of sum of total kinetic energy and total internal energy.
/// \todo this has to be generalized if some external potential is used.
class TotalEnergy : public IIntegral<Float> {
private:
    Vector omega;

public:
    explicit TotalEnergy(const Float omega = 0._f);

    virtual Float evaluate(const Storage& storage) const override;

    virtual String getName() const override {
        return "Total energy";
    }
};

/// \brief Computes the center of mass of particles.
///
/// By default, the center of mass is computed from all particles, optionally only particles belonging to body
/// of given ID are considered. The center is evaluated with a respect to reference frame.
class CenterOfMass : public IIntegral<Vector> {
private:
    Optional<Size> bodyId;

public:
    explicit CenterOfMass(const Optional<Size> bodyId = NOTHING);

    virtual Vector evaluate(const Storage& storage) const override;

    virtual String getName() const override {
        return "Center of mass";
    }
};

/// \brief Interface for auxilirary user-defined scalar quantities.
///
/// The quantity values needs to be computed from other quantities already stored in Storage. The class is
/// therefore suitable for quantities such as velocity magnitude, second invariant of stress tensor, etc.
class IUserQuantity : public Polymorphic {
public:
    virtual void initialize(const Storage& storage) = 0;

    virtual Float evaluate(const Size i) const = 0;

    virtual String name() const = 0;
};

/// \brief Returns means of given scalar quantity.
///
/// By default means are computed from all particles, optionally only from particles of given body. Storage
/// must contain quantity of given ID, checked by assert.
class QuantityMeans : public IIntegral<MinMaxMean> {
private:
    Variant<QuantityId, AutoPtr<IUserQuantity>> quantity;
    Optional<Size> bodyId;

public:
    /// \brief Computes mean of quantity values.
    /// \param id Quantity ID of average quantity
    /// \param bodyId Optional body index; by default all particles are averaged.
    explicit QuantityMeans(const QuantityId id, const Optional<Size> bodyId = NOTHING);

    /// \brief Computes mean of user-defined function.
    explicit QuantityMeans(AutoPtr<IUserQuantity>&& func, const Optional<Size> bodyId = NOTHING);

    virtual MinMaxMean evaluate(const Storage& storage) const override;

    virtual String getName() const override {
        if (auto id = quantity.tryGet<QuantityId>()) {
            return getMetadata(id.value()).quantityName;
        } else {
            return quantity.get<AutoPtr<IUserQuantity>>()->name();
        }
    }
};

/// \brief Returns the quantity value of given particle.
///
/// Mainly used for debugging. Currently available only for scalar quantities.
class QuantityValue : public IIntegral<Float> {
private:
    QuantityId id;
    Size idx;

public:
    QuantityValue(const QuantityId id, const Size particleIdx);

    virtual Float evaluate(const Storage& storage) const override;

    virtual String getName() const override {
        return getMetadata(id).quantityName + " " + toString(idx);
    }
};

/// \brief Helper integral wrapping another integral and converting the returned value to scalar.
///
/// Works as type erasere, allowing to integrals without template parameters, store integrals of different
/// types in one container, etc.
class IntegralWrapper : public IIntegral<Float> {
private:
    /// As integrals are templated, we have to put one more indirection to store them
    Function<Dynamic(const Storage& storage)> closure;

    /// Cached name of the object. This is not optimal, because the name can theorically change, but well ...
    String name;

public:
    IntegralWrapper() = default;

    template <typename TIntegral>
    IntegralWrapper(AutoPtr<TIntegral>&& integral) {
        name = integral->getName();
        closure = [i = std::move(integral)](const Storage& storage) -> Dynamic { //
            return i->evaluate(storage);
        };
    }

    virtual Float evaluate(const Storage& storage) const override {
        return closure(storage).getScalar();
    }

    virtual String getName() const override {
        return name;
    }
};

NAMESPACE_SPH_END
