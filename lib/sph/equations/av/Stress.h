#pragma once

/// \file Stress.h
/// \brief Form of tensor artificial viscosity for SPH with stress tensor
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "sph/equations/EquationTerm.h"
#include "sph/kernel/KernelFactory.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

/// \brief Artificial stress for simulations including stress tensor
///
/// This artificial force attemps to resolve problems with tensile instability in SPH. It isn't a replacement
/// of 'standard' artificial viscosity, both terms serve different purposes and they complement each other.
/// The implementation more or less follows paper 'SPH without a tensile instability' by Monaghan \cite
/// Monaghan_1999.
/// \note This object cannot be used within Balsara switch
class StressAV : public Abstract::EquationTerm {
private:
    LutKernel<3> kernel;

    class Derivative : public DerivativeTemplate<Derivative> {
    private:
        SymmetrizeSmoothingLengths<LutKernel<3>> kernel;
        Float n;  ///< exponent of the weighting function
        Float xi; ///< multiplicative factor

        ArrayView<const Float> wp;
        ArrayView<const SymmetricTensor> as;
        ArrayView<const Float> m;
        ArrayView<const Vector> r, v;

        ArrayView<Vector> dv;
        ArrayView<Float> du;

    public:
        Derivative(const RunSettings& settings) {
            kernel = Factory::getKernel<3>(settings);
            n = settings.get<Float>(RunSettingsId::SPH_AV_STRESS_EXPONENT);
            xi = settings.get<Float>(RunSettingsId::SPH_AV_STRESS_FACTOR);
        }

        virtual void create(Accumulated& results) override {
            /// \todo must specify order here, or forbit creating more derivatives afterwards
            results.insert<Vector>(QuantityId::POSITIONS);
            results.insert<Float>(QuantityId::ENERGY);
        }

        virtual void initialize(const Storage& input, Accumulated& results) override {
            wp = input.getValue<Float>(QuantityId::INTERPARTICLE_SPACING_KERNEL);
            as = input.getValue<SymmetricTensor>(QuantityId::AV_STRESS);
            m = input.getValue<Float>(QuantityId::MASSES);
            ArrayView<const Vector> dummy;
            tie(r, v, dummy) = input.getAll<Vector>(QuantityId::POSITIONS);

            dv = results.getValue<Vector>(QuantityId::POSITIONS);
            du = results.getValue<Float>(QuantityId::ENERGY);
        }

        template <bool Symmetrize>
        INLINE void eval(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
            ASSERT(neighs.size() == grads.size());
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k];
                const Float w = kernel.value(r[i], r[j]);

                // weighting function
                const Float phi = xi * pow(w / wp[i], n);

                const Vector f = phi * as[i] * grads[k];
                const Float heating = 0.5_f * phi * dot(as[i] * (v[i] - v[j]), grads[k]);
                dv[i] += m[j] * f;
                du[i] += m[j] * heating;
                if (Symmetrize) {
                    dv[j] -= m[i] * f;
                    du[j] += m[i] * heating;
                }
            }
        }
    };


public:
    StressAV(const RunSettings& settings) {
        kernel = Factory::getKernel<3>(settings);
    }

    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<Derivative>(settings);
    }

    virtual void initialize(Storage& storage) override {
        ArrayView<SymmetricTensor> as = storage.getValue<SymmetricTensor>(QuantityId::AV_STRESS);
        ArrayView<TracelessTensor> s =
            storage.getPhysicalValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
        ArrayView<Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
        ArrayView<Float> rho = storage.getValue<Float>(QuantityId::DENSITY);

        /// \todo parallelize
        for (Size i = 0; i < p.size(); ++i) {
            // compute total stress tesor
            const SymmetricTensor sigma = SymmetricTensor(s[i]) - p[i] * SymmetricTensor::identity();

            // find eigenvectors and corresponding eigenvalues
            Tensor matrix;
            Vector sigma_diag;
            tieToTuple(matrix, sigma_diag) = eigenDecomposition(sigma);

            // for positive values of diagonal stress, compute artificial stress
            const Vector as_diag = -max(sigma_diag, Vector(0._f)) / sqr(rho[i]);

            // transform back to original coordinates
            as[i] = transform(SymmetricTensor(as_diag, Vector(0._f)), matrix.transpose());
        }
    }

    virtual void finalize(Storage& UNUSED(storage)) override {}

    virtual void create(Storage& storage, Abstract::Material& UNUSED(material)) const override {
        Quantity& q = storage.insert<Float>(QuantityId::INTERPARTICLE_SPACING_KERNEL, OrderEnum::ZERO, 0._f);
        ArrayView<Float> wp = q.getValue<Float>();
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
        for (Size i = 0; i < r.size(); ++i) {
            /// Delta p / h is assumed to be constant, so that W(Delta p) is constant
            wp[i] = kernel.value(Vector(r[i][H], 0._f, 0._f), r[i][H]);
        }
        storage.insert<SymmetricTensor>(QuantityId::AV_STRESS, OrderEnum::ZERO, SymmetricTensor::null());
    }
};

NAMESPACE_SPH_END
