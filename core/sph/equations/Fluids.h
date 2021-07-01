#pragma once

/// \file Fluids.h
/// \brief Equations for simulations of water and other fluids
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "quantities/IMaterial.h"
#include "sph/equations/DerivativeHelpers.h"
#include "sph/equations/EquationTerm.h"
#include "sph/kernel/Kernel.h"

NAMESPACE_SPH_BEGIN

/// Most of the stuff here comes from paper 'Versatile Surface Tension and Adhesion for SPH Fluids' by
/// Akinci et al. (2013) \cite Akinci_2013

/// \brief Helper kernel used to simulate Lennard-Jones forces.
///
/// Do not use as SPH kernel, it only uses the kernel interface to utilize LutKernel and avoid code
/// duplication.
class CohesionKernel {
private:
    // this kernel does not have to be normalized to 1, this constant is used only to shift practical values
    // of the surface tension coefficient to 1.
    static constexpr Float normalization = 32._f / PI;

public:
    INLINE Float valueImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        SPH_ASSERT(q >= 0);

        if (q < 0.5_f) {
            return normalization * (2._f * pow<3>(1._f - q) * pow<3>(q) - 1._f / 64._f);
        }
        if (q < 1._f) {
            return normalization * (pow<3>(1._f - q) * pow<3>(q));
        }
        return 0._f;
    }

    INLINE Float gradImpl(const Float UNUSED(qSqr)) const {
        // called by LutKernel, although the values are never used
        return 0._f;
    }

    INLINE Float radius() const {
        return 1._f;
    }
};

class CohesionDerivative : public AccelerationTemplate<CohesionDerivative> {
private:
    /// Surface tension coefficient
    Float gamma;

    /// Cohesion kernel (different than the SPH kernel)
    SymmetrizeSmoothingLengths<LutKernel<3>> kernel;

    ArrayView<const Vector> r;
    ArrayView<const Vector> n;

public:
    explicit CohesionDerivative(const RunSettings& settings)
        : AccelerationTemplate<CohesionDerivative>(settings)
        , kernel(CohesionKernel{}) {}

    INLINE void additionalCreate(Accumulated& UNUSED(results)) {}

    INLINE void additionalInitialize(const Storage& input, Accumulated& UNUSED(results)) {
        r = input.getValue<Vector>(QuantityId::POSITION);
        n = input.getValue<Vector>(QuantityId::SURFACE_NORMAL);

        /// \todo needs to be generalized for heterogeneous fluids
        gamma = input.getMaterial(0)->getParam<Float>(BodySettingsId::SURFACE_TENSION);
    }

    INLINE bool additionalEquals(const CohesionDerivative& UNUSED(other)) const {
        return true;
    }

    template <bool Symmetrize>
    INLINE Tuple<Vector, Float> eval(const Size i, const Size j, const Vector& UNUSED(grad)) {
        if (SPH_UNLIKELY(r[i] == r[j])) {
            return { Vector(0._f), 0._f };
        }
        const Vector dr = getNormalized(r[i] - r[j]);
        const Float C = kernel.value(r[i], r[j]);

        // cohesive term + surface area normalizing term
        const Vector f = -gamma * C * dr - gamma * (n[i] - n[j]);
        SPH_ASSERT(isReal(f));

        return { f, 0._f }; /// \todo heating?
    }
};

/// \brief Computes the color field of the fluid.
class ColorFieldDerivative : public DerivativeTemplate<ColorFieldDerivative> {
private:
    ArrayView<const Float> m, rho;
    ArrayView<const Vector> r;

    ArrayView<Vector> n;

public:
    explicit ColorFieldDerivative(const RunSettings& settings)
        : DerivativeTemplate<ColorFieldDerivative>(settings) {}

    INLINE void additionalCreate(Accumulated& results) {
        results.insert<Vector>(QuantityId::SURFACE_NORMAL, OrderEnum::ZERO, BufferSource::UNIQUE);
    }

    INLINE void additionalInitialize(const Storage& input, Accumulated& results) {
        r = input.getValue<Vector>(QuantityId::POSITION);
        tie(m, rho) = input.getValues<Float>(QuantityId::MASS, QuantityId::DENSITY);

        n = results.getBuffer<Vector>(QuantityId::SURFACE_NORMAL, OrderEnum::ZERO);
    }

    INLINE bool additionalEquals(const ColorFieldDerivative& UNUSED(other)) const {
        return true;
    }

    template <bool Symmetrize>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        n[i] += r[i][H] * m[j] / rho[j] * grad;
        if (Symmetrize) {
            n[j] -= r[j][H] * m[i] / rho[i] * grad;
        }
    }
};

class CohesionTerm : public IEquationTerm {
public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require(makeAuto<CohesionDerivative>(settings));
        derivatives.require(makeAuto<ColorFieldDerivative>(settings));
    }

    virtual void initialize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

    virtual void finalize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

    virtual void create(Storage& storage, IMaterial& UNUSED(material)) const override {
        storage.insert<Vector>(QuantityId::SURFACE_NORMAL, OrderEnum::ZERO, Vector(0._f));
    }
};

NAMESPACE_SPH_END
