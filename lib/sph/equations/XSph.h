#pragma once

/// \file XSph.h
/// \brief XSPH correction to the integration of particle positions
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "sph/equations/DerivativeHelpers.h"
#include "sph/equations/EquationTerm.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

/// \brief XSPH correction that (partially) averages the velocities over neighbouring particles.
///
/// This keeps particles ordered in absence of viscosity. See Monaghan 1992 (Annu. Rev. Astron. Astrophys.
/// 1992.30:543-74)
/// \todo This implementation is currently not consistent ContinuitySolver; different velocities should also
/// affect the continuity equations (density derivative). For self-consistent solutions, use XSPH corrected
/// velocities in continuity equation or use direct summation of density.

class XSph : public IEquationTerm {
private:
    class Derivative : public DerivativeTemplate<Derivative> {
    private:
        /// \todo avoid constructing new kernel for each thread
        SymmetrizeSmoothingLengths<LutKernel<DIMENSIONS>> kernel;

        ArrayView<const Vector> r, v;
        ArrayView<const Float> rho, m;
        ArrayView<Vector> dr;
        Float epsilon;

    public:
        explicit Derivative(const RunSettings& settings)
            : DerivativeTemplate<Derivative>(settings)
            , kernel(Factory::getKernel<DIMENSIONS>(settings)) {
            epsilon = settings.get<Float>(RunSettingsId::SPH_XSPH_EPSILON);
        }

        INLINE void additionalCreate(Accumulated& results) {
            results.insert<Vector>(QuantityId::XSPH_VELOCITIES, OrderEnum::ZERO, BufferSource::UNIQUE);
        }

        INLINE void additionalInitialize(const Storage& input, Accumulated& results) {
            dr = results.getBuffer<Vector>(QuantityId::XSPH_VELOCITIES, OrderEnum::ZERO);
            tie(rho, m) = input.getValues<Float>(QuantityId::DENSITY, QuantityId::MASS);
            ArrayView<const Vector> dummy;
            tie(r, v, dummy) = input.getAll<Vector>(QuantityId::POSITION);
        }

        INLINE bool additionalEquals(const Derivative& other) const {
            return epsilon == other.epsilon;
        }

        template <bool Symmetric>
        INLINE void eval(const Size i, const Size j, const Vector& UNUSED(grad)) {
            // this depends on v[i]-v[j], so it is zero for i==j
            const Vector f = epsilon * (v[j] - v[i]) / (0.5_f * (rho[i] + rho[j])) * kernel.value(r[i], r[j]);
            dr[i] += m[j] * f;
            if (Symmetric) {
                dr[j] -= m[i] * f;
            }
        }
    };

public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require(makeAuto<Derivative>(settings));
    }

    virtual void initialize(IScheduler& UNUSED(scheduler), Storage& storage, const Float UNUSED(t)) override {
        // fix previously modified velocities before computing derivatives
        /// \todo this is not very good solution as it depends on ordering of equation term in the array;
        /// some may already get corrected velocities.
        /// This should be really done by deriving the solver and correcting velocities manually.
        ArrayView<Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
        ArrayView<Vector> dr = storage.getValue<Vector>(QuantityId::XSPH_VELOCITIES);
        for (Size i = 0; i < v.size(); ++i) {
            v[i] -= dr[i];
        }
    }

    virtual void finalize(IScheduler& UNUSED(scheduler), Storage& storage, const Float UNUSED(t)) override {
        ArrayView<Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
        ArrayView<Vector> dr = storage.getValue<Vector>(QuantityId::XSPH_VELOCITIES);
        for (Size i = 0; i < v.size(); ++i) {
            v[i] += dr[i];
        }
    }

    virtual void create(Storage& storage, IMaterial& UNUSED(material)) const override {
        storage.insert<Vector>(QuantityId::XSPH_VELOCITIES, OrderEnum::ZERO, Vector(0._f));
    }
};

NAMESPACE_SPH_END
