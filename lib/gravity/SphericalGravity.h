#pragma once

/// \file SphericalGravity.h
/// \brief Simple model of gravity, valid only for homogeneous spheres
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "gravity/IGravity.h"
#include "physics/Functions.h"
#include "sph/Materials.h"
#include "sph/equations/EquationTerm.h"

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

    virtual void build(const Storage& storage) override {
        r = storage.getValue<Vector>(QuantityId::POSITION);
        rho0 = storage.getMaterial(0)->getParam<Float>(BodySettingsId::DENSITY);
    }

    virtual void evalAll(ArrayView<Vector> dv, Statistics& UNUSED(stats)) const override {
        Analytic::StaticSphere sphere(INFTY, rho0); // here radius does not matter
        for (Size i = 0; i < dv.size(); ++i) {
            dv[i] += sphere.getAcceleration(r[i] - center);
        }
    }

    virtual void evalAll(ThreadPool& pool, ArrayView<Vector> dv, Statistics& UNUSED(stats)) const override {
        Analytic::StaticSphere sphere(INFTY, rho0);
        parallelFor(pool, 0, dv.size(), [this, sphere, &dv](const Size i) { //
            dv[i] += sphere.getAcceleration(r[i] - center);
        });
    }

    virtual Vector eval(const Vector& r0, Statistics& UNUSED(stats)) const override {
        Analytic::StaticSphere sphere(INFTY, rho0);
        return sphere.getAcceleration(r0 - center);
    }
};

/// \brief Implements IEquationTerm interface using SphericalGravity.
///
/// Useful for solvers that only accept equation terms, such as StaticSolver.
class SphericalGravityEquation : public IEquationTerm {
public:
    virtual void setDerivatives(DerivativeHolder& UNUSED(derivatives),
        const RunSettings& UNUSED(settings)) override {}

    virtual void initialize(Storage& UNUSED(storage), ThreadPool& UNUSED(pool)) override {}

    virtual void finalize(Storage& storage, ThreadPool& UNUSED(pool)) override {
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
