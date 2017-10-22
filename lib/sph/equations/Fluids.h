#prag1ma once

/// \file Fluids.h
/// \brief Equations for simulations of water and other fluids
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

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
    constexpr Float normalization = 32.f / PI;

public:
    INLINE Float valueImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        ASSERT(q >= 0);

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

class CohesionDerivative : public DerivativeTemplate<CohesionDerivative> {
private:
    /// Surface tension coefficient
    Float gamma;

    /// Cohesion kernel (different than the SPH kernel)
    SymmetrizeSmoothingLengths<LutKernel<3>> kernel;

    ArrayView<const Vector> r;
    ArrayView<const Float> m;
    ArrayView<const Vector> n;
    ArrayView<Vector> dv;

public:
    CohesionDerivative()
        : kernel(CohesionKernel{}) {}

    virtual void create(Accumulated& results) override {
        results.insert<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        r = input.getValue<Vector>(QuantityId::POSITIONS);
        n = input.getValue<Vector>(QuantityId::SURFACE_NORMAL);
        m = input.getValue<Float>(QuantityId::MASSES);
        dv = results.getBuffer<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND);

        /// \todo needs to be generalized for heterogeneous fluids
        gamma = input.getMaterial(0)->getParam<Float>(BodySettingsId::SURFACE_TENSION);
    }

    template <bool Symmetrize>
    INLINE void eval(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];
            const Vector dr = getNormalized(r[i] - r[j]);
            const Float C = kernel.value(r[i], r[j]);

            // cohesive term + surface area normalizing term
            const Vector f = -gamma * C * dr - gamma * (n[i] - n[j]);

            dv[i] += m[j] * f;
            if (Symmetrize) {
                dv[j] -= m[i] * f;
            }
        }
    }
};

/// \brief Computes the color field of the fluid.
class ColorField : public DerivativeTemplate<ColorField> {
private:
    ArrayView<const Float> m, rho;
    ArrayView<const Vector> r;

    ArrayView<Vector> n;

public:
    virtual void create(Accumulated& results) override {
        results.insert<Vector>(QuantityId::SURFACE_NORMAL, OrderEnum::ZERO);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        r = input.getValue<Vector>(QuantityId::POSITIONS);
        tie(m, rho) = input.getValues<Float>(QuantityId::MASSES, QuantityId::DENSITY);

        n = results.getBuffer<Vector>(QuantityId::SURFACE_NORMAL, OrderEnum::ZERO);
    }

    template <bool Symmetrize>
    INLINE void eval(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];

            n[i] += h[i] * m[j] / rho[j] * grads[k];
            if (Symmetrize) {
                n[j] -= h[j] * m[i] / rho[i] * grads[k];
            }
        }
    }
};

class CohesionTerm : public IEquationTerm {
private:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<CohesionDerivative>(settings);
        derivatives.require<ColorField>(settings);
    }

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& UNUSED(storage)) override {}

    virtual void create(Storage& storage, IMaterial& UNUSED(material)) const {
        storage.insert<Vector>(QuantityId::SURFACE_NORMAL, OrderEnum::ZERO, Vector(0._f));
    }
};

NAMESPACE_SPH_END
