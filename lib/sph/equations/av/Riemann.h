#pragma once

/// \file Riemann.h
/// \brief Artificial viscosity based on Riemann solver
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "quantities/Storage.h"
#include "sph/equations/DerivativeHelpers.h"
#include "sph/equations/EquationTerm.h"

NAMESPACE_SPH_BEGIN

/// \brief Artificial viscosity based on Riemann solver
///
/// See Monaghan (1997), SPH and Riemann Solvers, J. Comput. Phys. 136, 298
class RiemannAV : public IEquationTerm {
public:
    class Derivative : public AccelerationTemplate<Derivative> {
    private:
        Float alpha;
        ArrayView<const Vector> r, v;
        ArrayView<const Float> cs, rho;

    public:
        explicit Derivative(const RunSettings& settings)
            : AccelerationTemplate<Derivative>(settings) {
            alpha = settings.get<Float>(RunSettingsId::SPH_AV_ALPHA);
        }

        INLINE void additionalCreate(Accumulated& UNUSED(results)) {}

        INLINE void additionalInitialize(const Storage& storage, Accumulated& UNUSED(results)) {
            ArrayView<const Vector> dummy;
            tie(r, v, dummy) = storage.getAll<Vector>(QuantityId::POSITION);
            cs = storage.getValue<Float>(QuantityId::SOUND_SPEED);
            rho = storage.getValue<Float>(QuantityId::SOUND_SPEED);
        }

        INLINE bool additionalEquals(const Derivative& other) const {
            return alpha == other.alpha;
        }

        template <bool Symmetrize>
        INLINE Tuple<Vector, Float> eval(const Size i, const Size j, const Vector& grad) {
            const Float av = evalAv(i, j);
            ASSERT(isReal(av) && av >= 0._f);
            const Vector Pi = av * grad;
            const Float heating = 0.5_f * av * dot(v[i] - v[j], grad);
            ASSERT(isReal(heating) && heating >= 0._f);
            return { -Pi, heating };
        }

        INLINE Float evalAv(const int i, const int j) {
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
        derivatives.require(makeAuto<Derivative>(settings));
    }

    virtual void initialize(IScheduler& UNUSED(scheduler), Storage& UNUSED(storage)) override {}

    virtual void finalize(IScheduler& UNUSED(scheduler), Storage& UNUSED(storage)) override {}

    virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {}
};


NAMESPACE_SPH_END
