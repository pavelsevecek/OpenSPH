#pragma once

/// \file Standard.h
/// \brief Standard SPH artificial viscosity
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "physics/Eos.h"
#include "quantities/Storage.h"
#include "sph/equations/EquationTerm.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

/// \brief Standard artificial viscosity Monaghan & Gingold \cite Monaghan_Gingold_1983.
///
/// The artificial viscosity term uses a velocity divergence in linear and quadratic term as a measure of
/// local (scalar) dissipation. Acceleration due to the term is:
/// \f[
///  \frac{{\rm d} \vec v_i}{{\rm d} t} = \sum_j  \frac{m_j}{\bar\rho} \left(-\alpha_{\rm AV} \bar{c_{\rm
///  s}} \mu + \beta_{\rm AV} \mu^2 \right) \nabla W_{ij}\,,
/// \f]
/// where \f$\mu\f$ is defined as:
/// \f[
///  \mu = \frac{\bar{h}  (\vec v_i-\vec v_j)\cdot(\vec r_i-\vec r_j)}{ \epsilon \bar{h}^2 \| \vec r_i - \vec
///  r_j \| }\,.
/// \f]
/// A bar over values denodes symmetrization over particle pair, for example \f$\bar{h} = 0.5(h_i + h_j)\f$.
///
/// The viscosity only applies in convergent flow (\f$(\vec v_i-\vec v_j)\cdot(\vec r_i-\vec r_j) < 0\f$), it
/// is zero in divergent flow. Parameters \f$\alpha_{\rm AV}\f$ and \f$\beta_{\rm AV}\f$ are constant (in
/// time) and equal for all particles.
class StandardAV : public IEquationTerm {
public:
    class Derivative : public DerivativeTemplate<Derivative> {
    private:
        ArrayView<const Vector> r, v;
        ArrayView<const Float> rho, cs, m;
        ArrayView<Float> du;
        ArrayView<Vector> dv;
        const Float eps = 1.e-2_f;
        Float alpha, beta;

    public:
        explicit Derivative(const RunSettings& settings)
            : alpha(settings.get<Float>(RunSettingsId::SPH_AV_ALPHA))
            , beta(settings.get<Float>(RunSettingsId::SPH_AV_BETA)) {}

        virtual void create(Accumulated& results) override {
            results.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
            results.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST);
        }

        virtual void initialize(const Storage& input, Accumulated& results) override {
            ArrayView<const Vector> dummy;
            tie(r, v, dummy) = input.getAll<Vector>(QuantityId::POSITION);
            // sound speed must be computed by the solver using AV
            tie(rho, cs, m) =
                input.getValues<Float>(QuantityId::DENSITY, QuantityId::SOUND_SPEED, QuantityId::MASS);

            dv = results.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
            du = results.getBuffer<Float>(QuantityId::ENERGY, OrderEnum::FIRST);
        }

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

        /// Term used for Balsara switch
        INLINE Float operator()(const Size i, const Size j) const {
            const Float dvdr = dot(v[i] - v[j], r[i] - r[j]);
            if (dvdr >= 0._f) {
                return 0._f;
            }
            const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
            const Float rhobar = 0.5_f * (rho[i] + rho[j]);
            const Float csbar = 0.5_f * (cs[i] + cs[j]);
            const Float mu = hbar * dvdr / (getSqrLength(r[i] - r[j]) + eps * sqr(hbar));
            return 1._f / rhobar * (-alpha * csbar * mu + beta * sqr(mu));
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
