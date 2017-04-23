#pragma once

/// Solver for terms due to pressure gradient in equation of motion and energy equation

#include "geometry/Vector.h"
#include "objects/containers/ArrayView.h"
#include "objects/finders/AbstractFinder.h"
#include "quantities/Storage.h"
#include "solvers/Accumulated.h"
#include "system/Settings.h"
#include <map>

NAMESPACE_SPH_BEGIN


namespace Abstract {
    /// Derivative accumulated by summing up neighbouring particles. If solver is parallelized, each thread
    /// has its own derivatives that are summed after the solver loop.
    class Derivative : public Polymorphic {
    public:
        /// Emplace all needed buffers into shared storage. Called only once at the beginning of the run.
        virtual void create(Accumulated& results) = 0;

        /// Initialize derivative before iterating over neighbours.
        /// \param input Storage containing all the input quantities from which derivatives are computed.
        ///              This storage is shared for all threads.
        /// \param results Thread-local storage where the computed derivatives are saved.
        virtual void initialize(const Storage& input, Accumulated& results) = 0;

        /// Compute derivatives from interaction of particle pairs. Each particle pair is visited only once,
        /// the derivatives shall be computed for both particles.
        /// \param idx Index of first interacting particle
        /// \param neighs Array of neighbours of idx-th particle
        /// \param grads Computed gradients of the SPH kernel
        virtual void compute(const Size idx, ArrayView<const Size> neighs, ArrayView<const Vector> grads) = 0;
    };
}

template <typename Type, QuantityId Id, typename TDerived>
class VelocityTemplate : public Abstract::Derivative {
private:
    ArrayView<const Float> rho, m;
    ArrayView<const Vector> v;
    ArrayView<Type> deriv;

public:
    virtual void create(Accumulated& results) override {
        results.insert<Type>(Id);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        tie(rho, m) = input.getValues<Float>(QuantityId::DENSITY, QuantityId::MASSES);
        v = input.getDt<Vector>(QuantityId::POSITIONS);
        deriv = results.getValue<Type>(Id);
    }

    virtual void compute(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) override {
        ASSERT(neighs.size() == grads.size());
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];
            const auto dv = TDerived::func(v[j] - v[i], grads[k]);
            deriv[i] += m[j] / rho[j] * dv;
            deriv[j] += m[i] / rho[i] * dv;
        }
    }
};

struct VelocityDivergence
    : public VelocityTemplate<Float, QuantityId::VELOCITY_DIVERGENCE, VelocityDivergence> {
    INLINE static Float func(const Vector& dv, const Vector& grad) {
        return dot(dv, grad);
    }
};

struct VelocityGradient : public VelocityTemplate<Tensor, QuantityId::VELOCITY_GRADIENT, VelocityGradient> {
    INLINE static Tensor func(const Vector& dv, const Vector& grad) {
        return outer(dv, grad);
    }
};

struct VelocityRotation : public VelocityTemplate<Vector, QuantityId::VELOCITY_ROTATION, VelocityRotation> {
    INLINE static Vector func(const Vector& dv, const Vector& grad) {
        return cross(dv, grad);
    }
};

namespace VelocityGradientCorrection {
    struct NoCorrection {
        INLINE Vector operator()(const Size UNUSED(i), const Vector& grad) {
            return grad;
        }

        void initialize(const Storage& UNUSED(input)) {}
    };
}

template <typename TCorrection = VelocityGradientCorrection::NoCorrection>
class StrengthVelocityGradient : public Abstract::Derivative {
private:
    ArrayView<const Float> rho, m;
    ArrayView<const Vector> v;
    ArrayView<const Size> idxs;
    ArrayView<const TracelessTensor> s;
    ArrayView<Tensor> deriv;
    TCorrection correction;

public:
    virtual void create(Accumulated& results) override {
        results.insert<Tensor>(QuantityId::STRENGTH_VELOCITY_GRADIENT);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        tie(rho, m) = input.getValues<Float>(QuantityId::DENSITY, QuantityId::MASSES);
        v = input.getDt<Vector>(QuantityId::POSITIONS);
        idxs = input.getValue<Size>(QuantityId::FLAG);
        s = input.getPhysicalValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
        deriv = results.getValue<Tensor>(QuantityId::STRENGTH_VELOCITY_GRADIENT);
        correction.initialize(input);
    }

    virtual void compute(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) override {
        ASSERT(neighs.size() == grads.size());
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];
            /// \todo this is currently taken from SPH5 to allow easier comparison, drawback is that the
            /// density (and smoothing length) is evolved using this derivative or velocity divergence
            /// depending on damage status, it would be better to but this heuristics directly into the
            /// derivative, if possible
            if (idxs[i] != idxs[j] || s[i] == TracelessTensor::null() || s[j] == TracelessTensor::null()) {
                continue;
            }
            const Vector dv = v[j] - v[i];
            deriv[i] += m[j] / rho[j] * outer(dv, correction(i, grads[k]));
            deriv[j] += m[i] / rho[i] * outer(dv, correction(j, grads[k]));
        }
    }
};

/// Correction tensor to improve conservation of total angular momentum. This is only useful from stress
/// tensor is used in equation of motion, as total angular momentum is conversed by SPH discretization when
/// only pressure gradient is used.
///
/// The computed tensor C_ij is used to modify the total velocity gradient (and subsequently divergence and
/// rotation): the summed velocity divergences are multiplied by the inner product of kernel gradient and the
/// inverse of C_ij. This means that for self-consistent solution we need to do two passes over particle
/// pairs. However, this is only a numerical correction anyway, so we can use values from previous timestep,
/// like we do for any time-dependent quantities.
///
/// See paper 'Collisions between equal-sized ice grain agglomerates' by Schafer et al. (2007).
class AngularMomentumCorrectionTensor : public Abstract::Derivative {
private:
    ArrayView<const Float> m;
    ArrayView<const Float> rho;
    ArrayView<const Vector> r;
    ArrayView<Tensor> C;

public:
    virtual void create(Accumulated& results) override {
        results.insert<Tensor>(QuantityId::ANGULAR_MOMENTUM_CORRECTION);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        tie(m, rho) = input.getValues<Float>(QuantityId::MASSES, QuantityId::DENSITY);
        r = input.getValue<Vector>(QuantityId::POSITIONS);
        C = results.getValue<Tensor>(QuantityId::ANGULAR_MOMENTUM_CORRECTION);
    }

    virtual void compute(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) override {
        ASSERT(neighs.size() == grads.size());
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];
            Tensor t = outer(r[j] - r[i], grads[k]); // symmetric in i,j ?
            C[i] += m[j] / rho[j] * t;
            C[j] += m[i] / rho[i] * t;
        }
    }
};

namespace VelocityGradientCorrection {
    class ConserveAngularMomentum {
    private:
        ArrayView<const Tensor> C_inv;

    public:
        INLINE Vector operator()(const Size i, const Vector& grad) {
            return C_inv[i] * grad;
        }

        void initialize(const Storage& input) {
            C_inv = input.getValue<Tensor>(QuantityId::ANGULAR_MOMENTUM_CORRECTION);
        }
    };
}


class DerivativeHolder {
private:
    Accumulated accumulated;
    /// Holds all modules that are evaluated in the loop. Modules save data to the \ref accumulated
    /// storage; one module can use multiple buffers (acceleration and energy derivative) and multiple
    /// modules can write into same buffer (different terms in equation of motion).
    /// Modules are evaluated consecutively (within one thread), so this is thread-safe.
    Array<std::unique_ptr<Abstract::Derivative>> derivatives;

public:
    /// Adds derivative if not already present. If the derivative is already stored, new one is NOT
    /// created, even if settings are different.
    template <typename TDerivative, typename... TArgs>
    void require(TArgs&&... args) {
        for (auto& d : derivatives) {
            if (typeid(*d) == typeid(TDerivative)) {
                return;
            }
        }
        std::unique_ptr<TDerivative> ptr = std::make_unique<TDerivative>(std::forward<TArgs>(args)...);
        derivatives.push(std::move(ptr));
    }

    /// Initialize derivatives before loop
    void initialize(const Storage& input) {
        if (accumulated.getBufferCnt() == 0) {
            // lazy buffer creation
            /// \todo this will be called every time if derivatives do not create any buffers,
            /// does it really matter?
            for (auto& deriv : derivatives) {
                deriv->create(accumulated);
            }
        }
        // initialize buffers first, possibly resizing then and invalidating previously stored arrayviews
        accumulated.initialize(input.getParticleCnt());
        for (auto& deriv : derivatives) {
            // then get the arrayviews for derivatives
            deriv->initialize(input, accumulated);
        }
    }

    void compute(const Size idx, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
        ASSERT(neighs.size() == grads.size());
        for (auto& deriv : derivatives) {
            deriv->compute(idx, neighs, grads);
        }
    }

    Accumulated& getAccumulated() {
        return accumulated;
    }

    Size getDerivativeCnt() const {
        return derivatives.size();
    }
};

NAMESPACE_SPH_END
