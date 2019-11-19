#pragma once

/// \file Potentials.h
/// \brief Additional forces that do not depend on spatial derivatives
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "physics/Functions.h"
#include "sph/equations/EquationTerm.h"

NAMESPACE_SPH_BEGIN

/// \brief Generic external force given by lambda function
///
/// Adds an acceleration term to every particle given by user-specified function. The term can only depend on
/// particle positions (not speed nor any other quantity). Energy is not modified by the force.
/// The term is given in template parameter of the class, takes particle position as an argument and returns
/// the acceleration. The type be copyable or moveable as the functor is stored as member variable.
///
/// To utilize type deduction, use helper function \ref makeExternalForce
template <typename TFunctor>
class ExternalForce : public IEquationTerm {
private:
    std::remove_reference_t<TFunctor> functor;

public:
    ExternalForce(TFunctor&& functor)
        : functor(std::forward<TFunctor>(functor)) {}

    virtual void setDerivatives(DerivativeHolder& UNUSED(derivatives),
        const RunSettings& UNUSED(settings)) override {}

    virtual void initialize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

    virtual void finalize(IScheduler& UNUSED(scheduler), Storage& storage, const Float t) override {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
        /// \todo parallelize
        for (Size i = 0; i < r.size(); ++i) {
            dv[i] += functor(r[i], t);
        }
    }

    virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {}
};

/// Helper function to create ExternalForce utilizing type deduction. This way, we can use lambda as the
/// template functor.
template <typename TFunctor>
EquationHolder makeExternalForce(TFunctor&& functor) {
    return EquationHolder(makeShared<ExternalForce<TFunctor>>(std::forward<TFunctor>(functor)));
}

/// \brief Centrifugal and Coriolis force
///
/// Adds an acceleration due to centrifugal force. Internal energy is not modified by this force.
class InertialForce : public IEquationTerm {
private:
    Vector omega;

public:
    InertialForce(const Vector omega)
        : omega(omega) {}

    virtual void setDerivatives(DerivativeHolder& UNUSED(derivatives),
        const RunSettings& UNUSED(settings)) override {}

    virtual void initialize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

    virtual void finalize(IScheduler& UNUSED(scheduler), Storage& storage, const Float UNUSED(t)) override {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
        /// \todo parallelize
        for (Size i = 0; i < r.size(); ++i) {
            dv[i] -= 2._f * cross(omega, v[i]) + cross(omega, cross(omega, r[i]));
            // no energy term - energy is not generally conserved when external force is used
        }
    }

    virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {}
};

NAMESPACE_SPH_END
