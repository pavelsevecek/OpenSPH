#pragma once

/// \file GradH.h
/// \brief Corrections due to gradient of smoothing length (grad-h terms)
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "sph/equations/EquationTerm.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

/// \todo few problems with current implementation:
/// 1) the kernel should really be shared with the rest of the solver
/// 2) using grad-h terms is a SUBSTITUTE for symmetrized kernel; either one or the other should be used, not
///    both together
class GradH : public IDerivative {
private:
    /// \todo avoid constructing new kernel for each thread
    LutKernel<DIMENSIONS> kernel;

    ArrayView<const Vector> r;
    ArrayView<const Float> rho;
    ArrayView<Float> omega;

public:
    explicit GradH(const RunSettings& settings)
        : kernel(Factory::getKernel<DIMENSIONS>(settings)) {}

    virtual DerivativePhase phase() const override {
        return DerivativePhase::PRECOMPUTE;
    }

    virtual void create(Accumulated& results) override {
        results.insert<Float>(QuantityId::GRAD_H, OrderEnum::ZERO, BufferSource::UNIQUE);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        omega = results.getBuffer<Float>(QuantityId::GRAD_H, OrderEnum::ZERO);
        rho = input.getValue<Float>(QuantityId::DENSITY);
        r = input.getValue<Vector>(QuantityId::POSITION);
    }

    virtual void evalNeighs(const Size i,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> UNUSED(grads)) override {
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];
            const Vector r_ji = r[j] - r[i];
            const Float h_j = r[j][H];
            const Float dWij_dh =
                -dot(r_ji, kernel.grad(r_ji, h_j)) - DIMENSIONS / h_j * kernel.value(r_ji, h_j);
            omega[i] += dWij_dh;
        }
        // add term for i=j (right?)
        omega[i] += -DIMENSIONS / r[i][H] * kernel.value(Vector(0._f), r[i][H]);

        omega[i] = 1._f + r[i][H] / (3._f * rho[i]) * omega[i];
    }
};


NAMESPACE_SPH_END
