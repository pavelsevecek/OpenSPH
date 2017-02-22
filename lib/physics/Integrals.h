#pragma once

#include "math/Means.h"
#include "objects/ForwardDecl.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/Value.h"
#include "system/Settings.h"
#include <memory>

NAMESPACE_SPH_BEGIN


/// Object computing integral quantities and diagnostics of the run.
/// \todo automatically exclude ghost particles?
namespace Abstract {

    template <typename Type>
    class Integral {
    public:
        /// Computes the integral quantity using particles in the storage.
        virtual Type evaluate(Storage& storage) const = 0;
    };
}


/// Computes the total mass of all SPH particles. Storage must contains particle masses, of course;
/// checked by assert.
/// \note Total mass is always conserved automatically as particles do not change their mass. This is
/// therefore only useful as a sanity check, or potentially if a solver with variable particle masses
/// gets implemented.
class TotalMass : public Abstract::Integral<Float> {
public:
    virtual Float evaluate(Storage& storage) const override;
};

/// Computes total momentum of all SPH particles with a respect to the center of reference frame.
/// Storage must contain at least particle masses and particle positions with velocities, checked by
/// assert.
class TotalMomentum : public Abstract::Integral<Vector> {
private:
    Vector omega;

public:
    TotalMomentum(const Float omega = 0._f);

    virtual Vector evaluate(Storage& storage) const override;
};

/// Computes total angular momentum of all SPH particles with a respect to the center of reference
/// frame. Storage must contain at least particle masses and particle positions with velocities, checked by
/// assert.
class TotalAngularMomentum : public Abstract::Integral<Vector> {
private:
    Vector omega;

public:
    TotalAngularMomentum(const Float omega = 0._f);

    virtual Vector evaluate(Storage& storage) const override;
};

/// Returns the total kinetic energy of all particles. Storage must contain at least particle masses
/// and particle positions with velocities, checked by assert.
class TotalKineticEnergy : public Abstract::Integral<Float> {
private:
    Vector omega;

public:
    TotalKineticEnergy(const Float omega = 0._f);

    virtual Float evaluate(Storage& storage) const override;
};

/// Returns the total internal energy of all particles. Storage must contain at least particle masses
/// and specific internal energy. If used solver works with other independent quantity (energy density, total
/// energy, specific entropy), specific energy must be derived before the function is called.
class TotalInternalEnergy : public Abstract::Integral<Float> {
public:
    virtual Float evaluate(Storage& storage) const override;
};

/// Returns the total energy of all particles. This is simply of sum of total kinetic energy and total
/// internal energy.
/// \todo this has to be generalized if some external potential is used.
class TotalEnergy : public Abstract::Integral<Float> {
private:
    Vector omega;

public:
    TotalEnergy(const Float omega = 0._f);

    virtual Float evaluate(Storage& storage) const override;
};

/// Computes the center of mass of all particles, or optionally center of mass of particles
/// belonging to body of given ID. The center is evaluated with a respect to reference frame.
class CenterOfMass : public Abstract::Integral<Vector> {
private:
    Optional<Size> bodyId;

public:
    CenterOfMass(const Optional<Size> bodyId = NOTHING);

    virtual Vector evaluate(Storage& storage) const override;
};

/// Returns means of given scalar quantity. By default means are computed from all particles, optionally only
/// from particles of given body. Storage must contain quantity of given ID, checked by assert.
class QuantityMeans : public Abstract::Integral<Means> {
private:
    Variant<QuantityIds, std::function<Float(const Size i)>> quantity;
    Optional<Size> bodyId;

public:
    /// Computes mean of quantity values.
    QuantityMeans(const QuantityIds id, const Optional<Size> bodyId = NOTHING);

    /// Computes mean of user-defined function.
    QuantityMeans(const std::function<Float(const Size i)>& func, const Optional<Size> bodyId = NOTHING);

    virtual Means evaluate(Storage& storage) const override;
};

/// Returns the quantity value value of given particle. Currently available only for scalar quantities.
class QuantityValue : public Abstract::Integral<Float> {
private:
    QuantityIds id;
    Size idx;

public:
    QuantityValue(const QuantityIds id, const Size particleIdx);

    virtual Float evaluate(Storage& storage) const override;
};

NAMESPACE_SPH_END
