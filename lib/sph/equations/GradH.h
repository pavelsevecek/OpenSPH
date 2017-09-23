#pragma once

/// \file GradH.h
/// \brief Corrections due to gradient of smoothing length (grad-h terms)
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "sph/equations/EquationTerm.h"
#include "sph/kernel/KernelFactory.h"

NAMESPACE_SPH_BEGIN

/// \todo few problems with current implementation:
/// 1) the kernel should really be shared with the rest of the solver
/// 2) grad-h terms should be computed in the same step, by iterating over neighbouring particles twice; first
///    to get grad-h terms, second to compute all derivatives corrected by grad-h terms.
/// 3) using grad-h terms is a SUBSTITUTE for symmetrized kernel; either one or the other should be used, not
///    both together
/// 4) this breaks the symmetrized computation of interactions, we need to evaluated kernel gradient for BOTH
///    particles anyway
///
/// To use grad-h properly, it would be probably necessary to create new solver and also modify equations
/// (derivatives) affected by grad-h terms (uff...)

class GradH : public IEquationTerm {
private:
    class Derivative : public DerivativeTemplate<Derivative> {
    private:
        /// \todo avoid constructing new kernel for each thread
        LutKernel<DIMENSIONS> kernel;

        ArrayView<const Vector> r;
        ArrayView<const Float> rho;
        ArrayView<Float> omega;

    public:
        explicit Derivative(const RunSettings& settings)
            : kernel(Factory::getKernel<DIMENSIONS>(settings)) {}

        virtual void create(Accumulated& results) override {
            results.insert<Float>(QuantityId::GRAD_H, OrderEnum::ZERO);
        }

        virtual void initialize(const Storage& input, Accumulated& results) override {
            omega = results.getBuffer<Float>(QuantityId::GRAD_H, OrderEnum::ZERO);
            rho = input.getValue<Float>(QuantityId::DENSITY);
            r = input.getValue<Vector>(QuantityId::POSITIONS);
        }

        template <bool Symmetrize>
        INLINE void eval(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> UNUSED(grads)) {
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k];
                const Vector r_ji = r[j] - r[i];
                const Float h_j = r[j][H];
                const Float dWijdh =
                    -dot(r_ji, kernel.grad(r_ji, h_j)) - DIMENSIONS / h_j * kernel.value(r_ji, h_j);
                omega[i] += dWijdh;

                if (Symmetrize) {
                    const Float h_i = r[i][H];
                    const Float dWjidh =
                        -dot(r_ji, kernel.grad(r_ji, h_i)) - DIMENSIONS / h_i * kernel.value(r_ji, h_i);
                    omega[j] += dWjidh;
                }
            }
        }
    };

public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<Derivative>(settings);
    }

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& storage) override {
        ArrayView<Float> omega = storage.getValue<Float>(QuantityId::GRAD_H);
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
        ArrayView<Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
        // so far we only summed up the dW/dh terms, compute final grad-h term
        for (Size i = 0; i < omega.size(); ++i) {
            omega[i] = 1._f + r[i][H] / (3._f * rho[i]) * omega[i];
        }
    }

    virtual void create(Storage& storage, IMaterial& UNUSED(material)) const override {
        storage.insert<Float>(QuantityId::GRAD_H, OrderEnum::ZERO, 1._f);
    }
};


NAMESPACE_SPH_END
