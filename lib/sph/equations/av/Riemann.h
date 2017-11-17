#pragma once

/// \file Riemann.h
/// \brief Artificial viscosity based on Riemann solver
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "quantities/Storage.h"
#include "sph/equations/EquationTerm.h"

NAMESPACE_SPH_BEGIN

/// \brief Artificial viscosity based on Riemann solver
///
/// See Monaghan (1997), SPH and Riemann Solvers, J. Comput. Phys. 136, 298
class RiemannAV : public IEquationTerm {
public:
    class Derivative : public DerivativeTemplate<Derivative> {
    private:
        Float alpha;
        ArrayView<const Vector> r, v;
        ArrayView<const Float> cs, rho, m;

        ArrayView<Vector> dv;
        ArrayView<Float> du;

    public:
        explicit Derivative(const RunSettings& settings) {
            alpha = settings.get<Float>(RunSettingsId::SPH_AV_ALPHA);
        }

        virtual void create(Accumulated& results) override {
            results.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
            results.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST);
        }

        virtual void initialize(const Storage& storage, Accumulated& results) override {
            ArrayView<const Vector> dummy;
            tie(r, v, dummy) = storage.getAll<Vector>(QuantityId::POSITION);
            cs = storage.getValue<Float>(QuantityId::SOUND_SPEED);
            rho = storage.getValue<Float>(QuantityId::SOUND_SPEED);
            m = storage.getValue<Float>(QuantityId::MASS);

            dv = results.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
            du = results.getBuffer<Float>(QuantityId::ENERGY, OrderEnum::FIRST);
        }

        /// \todo can be de-duplicated, moving to common parent for all AVs
        template <bool Symmetrize>
        INLINE void eval(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
            ASSERT(neighs.size() == grads.size());
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k];
                const Float av = (*this)(i, j);
                ASSERT(isReal(av) && av >= 0._f);
                const Vector Pi = av * grads[k];
                const Float heating = 0.5_f * av * dot(v[i] - v[j], grads[k]);
                ASSERT(isReal(heating) && heating >= 0._f);
                dv[i] -= m[j] * Pi;
                du[i] += m[j] * heating;

                if (Symmetrize) {
                    dv[j] += m[i] * Pi;
                    du[j] += m[i] * heating;
                }
            }
        }

        INLINE Float operator()(const int i, const int j) {
            const Float dvdr = dot(v[i] - v[j], r[i] - r[j]);
            if (dvdr >= 0._f) {
                return 0._f;
            }
            const Float w = dvdr / getLength(r[i] - r[j]);
            const Float vsig = cs[i] + cs[j] - 3._f * w;
            const Float rhobar = 0.5_f * (rho[i] + rho[j]);
            return -0.5_f * alpha * vsig * w / rhobar;
        }
    };

    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<Derivative>(settings);
    }

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& UNUSED(storage)) override {}

    virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {}
};


NAMESPACE_SPH_END
