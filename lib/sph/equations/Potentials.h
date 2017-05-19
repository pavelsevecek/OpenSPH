#pragma once

/// \file Potentials.h
/// \brief Additional forces that do not depend on spatial derivatives
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

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
class ExternalForce : public Abstract::EquationTerm {
private:
    std::remove_reference_t<TFunctor> functor;

public:
    ExternalForce(TFunctor&& functor)
        : functor(std::forward<TFunctor>(functor)) {}

    virtual void setDerivatives(DerivativeHolder& UNUSED(derivatives),
        const RunSettings& UNUSED(settings)) override {}

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& storage) override {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
        /// \todo parallelize
        for (Size i = 0; i < r.size(); ++i) {
            dv[i] += functor(r[i]);
        }
    }

    virtual void create(Storage& UNUSED(storage), Abstract::Material& UNUSED(material)) const override {}
};

/// Helper function to create ExternalForce utilizing type deduction. This way, we can use lambda as the
/// template functor.
template <typename TFunctor>
AutoPtr<ExternalForce<TFunctor>> makeExternalForce(TFunctor&& functor) {
    return makeAuto<ExternalForce<TFunctor>>(std::forward<TFunctor>(functor));
}

/// \brief Centripetal force, assuming the frame is rotation around Z axis.
///
/// Adds an acceleration due to centripetal force. Internal energy is not modified by this force.
/// \todo Add Coriolis force -> NonintertialForce
class CentripetalForce : public Abstract::EquationTerm {
private:
    Float omega;

public:
    CentripetalForce(const Float omega)
        : omega(omega) {}

    virtual void setDerivatives(DerivativeHolder& UNUSED(derivatives),
        const RunSettings& UNUSED(settings)) override {}

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& storage) override {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
        const Vector unitZ = Vector(0._f, 0._f, 1._f);
        /// \todo parallelize
        for (Size i = 0; i < r.size(); ++i) {
            dv[i] += sqr(omega) * (r[i] - unitZ * dot(r[i], unitZ));
            // no energy term - energy is not generally conserved when external force is used
        }
    }

    virtual void create(Storage& UNUSED(storage), Abstract::Material& UNUSED(material)) const override {}
};


/// \brief Spherically symmetrized gravitational force.
///
/// Computes gravitational force of a sphere (not necessarily homogeneous). Particles are assumed to be
/// spherically symmetric; the force can be used even for different particle distributions, but may yield
/// incorrect results.
class SphericalGravity : public Abstract::EquationTerm {
public:
    virtual void setDerivatives(DerivativeHolder& UNUSED(derivatives),
        const RunSettings& UNUSED(settings)) override {}

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& storage) override {
        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASSES);
        Float rmax = 0._f;
        for (Size i = 0; i < r.size(); ++i) {
            rmax = max(rmax, getSqrLength(r[i]));
        }
        rmax = sqrt(rmax);
        ASSERT(isReal(rmax));

        // const Size binCnt = r.size() / 10;
        //        Array<Float> M(binCnt); // M(R)


        Array<Size> idxs(m.size());
        for (Size i = 0; i < idxs.size(); ++i) {
            idxs[i] = i;
        }
        // sort particles by increasing r
        std::sort(idxs.begin(), idxs.end(), [r](const Size i1, const Size i2) {
            return getLength(r[i1]) < getLength(r[i2]);
        });
        // compute mass M within radius r
        Array<Float> M(m.size() + 1);
        /// \todo replace this staircase function with some smooth spline
        M[0] = 0._f;
        for (Size i = 0; i < idxs.size(); ++i) {
            M[i + 1] = m[idxs[i]] + M[i];
        }
        // compute acceleration
        ArrayView<Vector> dv = storage.getD2t<Vector>(QuantityId::POSITIONS);
        for (Size i = 0; i < dv.size(); ++i) {
            const Size idx = idxs[i];
            dv[idx] -= Constants::gravity * M[i] * r[idx] / pow<3>(getLength(r[idx]));
        }
    }

    virtual void create(Storage& UNUSED(storage), Abstract::Material& UNUSED(material)) const override {}
};

NAMESPACE_SPH_END