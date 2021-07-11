#pragma once

/// \file SphericalGravity.h
/// \brief Simple model of gravity, valid only for homogeneous spheres
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "gravity/IGravity.h"
#include "physics/Functions.h"
#include "sph/Materials.h"
#include "sph/equations/EquationTerm.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

/// \brief Spherically symmetrized gravitational force.
///
/// Computes gravitational force of a homogeneous sphere. This is the fastest possible evaluation of gravity,
/// but it's very imprecise and limited only to spheres, so it is only intended to be used for simple tests
/// and sanity checks of more complex gravity models.
/// Particles are assumed to be spherically symmetric; the force can be used even for different particle
/// distributions, but may yield incorrect results.
class SphericalGravity : public IGravity {
private:
    Vector center;
    Float rho0;
    ArrayView<const Vector> r;

public:
    SphericalGravity(const Vector& center = Vector(0._f))
        : center(center) {}

    virtual void build(IScheduler& UNUSED(scheduler), const Storage& storage) override {
        r = storage.getValue<Vector>(QuantityId::POSITION);
        rho0 = storage.getMaterial(0)->getParam<Float>(BodySettingsId::DENSITY);
    }

    virtual void evalAll(IScheduler& scheduler,
        ArrayView<Vector> dv,
        Statistics& UNUSED(stats)) const override {
        Analytic::StaticSphere sphere(INFTY, rho0); // here radius does not matter
        parallelFor(scheduler, 0, dv.size(), [this, sphere, &dv](const Size i) {
            dv[i] += sphere.getAcceleration(r[i] - center);
        });
    }

    virtual Vector eval(const Vector& r0) const override {
        Analytic::StaticSphere sphere(INFTY, rho0);
        return sphere.getAcceleration(r0 - center);
    }

    virtual Float evalEnergy(IScheduler& UNUSED(scheduler), Statistics& UNUSED(stats)) const override {
        Analytic::StaticSphere sphere(INFTY, rho0);
        return sphere.getEnergy();
    }

    virtual RawPtr<const IBasicFinder> getFinder() const override {
        return nullptr;
    }
};

/// \brief Implements IEquationTerm interface using SphericalGravity.
///
/// Useful for solvers that only accept equation terms, such as EquilibriumSolver.
class SphericalGravityEquation : public IEquationTerm {
public:
    virtual void setDerivatives(DerivativeHolder& UNUSED(derivatives),
        const RunSettings& UNUSED(settings)) override {}

    virtual void initialize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

    virtual void finalize(IScheduler& UNUSED(scheduler), Storage& storage, const Float UNUSED(t)) override {
        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        ArrayView<Vector> dv = storage.getD2t<Vector>(QuantityId::POSITION);
        const Float rho0 = storage.getMaterial(0)->getParam<Float>(BodySettingsId::DENSITY);
        Analytic::StaticSphere sphere(INFTY, rho0);
        for (Size i = 0; i < dv.size(); ++i) {
            dv[i] += sphere.getAcceleration(r[i]);
        }
    }

    virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {}
};


NAMESPACE_SPH_END
