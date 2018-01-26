#pragma once

#include "sph/equations/EquationTerm.h"
#include "sph/kernel/Kernel.h"

NAMESPACE_SPH_BEGIN

namespace BenzAsphaugSph {
    class SolidStressTorque : public IEquationTerm {
    private:
        /// Dimensionless moment of inertia
        ///
        /// Computed by computing integral \f[\int r^2_\perp W(r) dV \f] for selected kernel
        Float inertia;

        bool evolveAngle;

    public:
        explicit SolidStressTorque(const RunSettings& settings);

        virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override;

        virtual void initialize(Storage& storage) override;

        virtual void finalize(Storage& storage) override;

        virtual void create(Storage& storage, IMaterial& material) const override;

        /// \brief Returns the dimensionless moment of inertia of particles.
        ///
        /// Exposed mainly for testing purposes.
        INLINE Float getInertia() const {
            return inertia;
        }
    };
} // namespace BenzAsphaugSph

namespace StandardSph {
    class SolidStressTorque : public BenzAsphaugSph::SolidStressTorque {
    public:
        virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override;
    };

} // namespace StandardSph

NAMESPACE_SPH_END
