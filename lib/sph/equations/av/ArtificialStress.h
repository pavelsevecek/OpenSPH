#pragma once

/// \file ArtificialStress.h
/// \brief Form of tensor artificial viscosity for SPH with stress tensor
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "sph/equations/EquationTerm.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

/// The implementation more or less follows paper 'SPH without a tensile instability' by Monaghan \cite
/// Monaghan_1999
/// \note This object cannot be used within Balsara switch
class ArtificialStress : public Abstract::EquationTerm {
private:
    LutKernel<3> kernel;

    class Derivative : public DerivativeTemplate<Derivative> {
    private:
        SymmetrizeSmoothingLengths<LutKernel<3>> kernel;
        Size n;

        ArrayView<const Float> wp;

    public:
        Derivative(const RunSettings& settings) {
            kernel = Factory::getKernel<3>(settings);
            n = settings.get<Float>(RunSettingsId::ARTIFICIAL_STRESS_EXPONENT);
        }

        virtual void create(Accumulated& results) override {}

        virtual void initialize(const Storage& input, Accumulated& results) override {
            wp = input.getValue<Float>(QuantityId::INTERPARTICLE_SPACING_KERNEL);
        }

        template <bool Symmetrize>
        INLINE void eval(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
            ASSERT(neighs.size() == grads.size());
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k];
                const Float w = kernel.value(r[i], r[j]);
                const Float phi = pow(w / wp[i], n);
            }
        }
    };


public:
    ArtificialStress(const RunSettings& settings) {
        kernel = Factory::getKernel<3>(settings);
    }

    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {}

    virtual void initialize(Storage& storage) override {
        ArrayView<Vector> s_diag = storage.getValue<Vector>(QuantityId::DIAGONAL_STRESS);
        ArrayView<TracelessTensor> s =
            storage.getPhysicalValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
        ArrayView<Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
        for (Size i = 0; i < p.size(); ++i) {
            const SymmetricTensor sigma = s - p * SymmetricTensor::identity();
            tie(s_diag[i][X], s_diag[i][Y], s_diag[i][Z]) = findEigenvalues(sigma);
        }
    }

    virtual void finalize(Storage& storage) override {}

    virtual void create(Storage& storage, Abstract::Material& material) const override {
        ArrayView<Float> wp =
            storage.insert<Float>(QuantityId::INTERPARTICLE_SPACING_KERNEL, OrderEnum::ZERO, 0._f);
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
        for (Size i = 0; i < r.size(); ++i) {
            /// Delta p / h is assumed to be constant, so that W(Delta p) is constant
            wp[i] = kernel.value(Vector(r[i][H], 0._f, 0._f), r[i][H]);
        }
        storage.insert<Vector>(QuantityId::DIAGONAL_STRESS, OrderEnum::ZERO, Vector(0._f));
    }
};

NAMESPACE_SPH_END
