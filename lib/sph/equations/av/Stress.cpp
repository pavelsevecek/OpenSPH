#include "sph/equations/av/Stress.h"
#include "sph/equations/DerivativeHelpers.h"
#include "sph/equations/EquationTerm.h"
#include "system/Factory.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

template <typename Discr>
class StressAV::Derivative : public AccelerationTemplate<Derivative<Discr>> {
private:
    SymmetrizeSmoothingLengths<LutKernel<3>> kernel;
    Float n;  ///< exponent of the weighting function
    Float xi; ///< multiplicative factor

    ArrayView<const Float> wp;
    ArrayView<const SymmetricTensor> as;
    ArrayView<const Float> rho;
    ArrayView<const Vector> r, v;

    Discr discr;

public:
    explicit Derivative(const RunSettings& settings)
        : AccelerationTemplate<Derivative<Discr>>(settings, DerivativeFlag::SUM_ONLY_UNDAMAGED) {
        kernel = Factory::getKernel<3>(settings);
        n = settings.get<Float>(RunSettingsId::SPH_AV_STRESS_EXPONENT);
        xi = settings.get<Float>(RunSettingsId::SPH_AV_STRESS_FACTOR);
    }

    INLINE void additionalCreate(Accumulated& UNUSED(results)) const {}

    INLINE void additionalInitialize(const Storage& input, Accumulated& UNUSED(results)) {
        wp = input.getValue<Float>(QuantityId::INTERPARTICLE_SPACING_KERNEL);
        as = input.getValue<SymmetricTensor>(QuantityId::AV_STRESS);
        rho = input.getValue<Float>(QuantityId::DENSITY);
        ArrayView<const Vector> dummy;
        tie(r, v, dummy) = input.getAll<Vector>(QuantityId::POSITION);
    }

    INLINE bool additionalEquals(const Derivative& other) const {
        return n == other.n && xi == other.xi;
    }

    template <bool Symmetrize>
    INLINE Tuple<Vector, Float> eval(const Size i, const Size j, const Vector& grad) {
        const Float w = kernel.value(r[i], r[j]);

        // weighting function
        const Float phi = xi * pow(w / wp[i], n);

        // AV term, discretized consistently with the selected SPH formulation
        const SymmetricTensor Pi = phi * discr(as[i], as[j], rho[i], rho[j]);
        const Vector f = Pi * grad;
        const Float heating = 0.5_f * dot(Pi * (v[i] - v[j]), grad);
        return { f, heating };
    }
};

StressAV::StressAV(const RunSettings& settings) {
    kernel = Factory::getKernel<3>(settings);
}

void StressAV::setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) {
    const DiscretizationEnum formulation =
        settings.get<DiscretizationEnum>(RunSettingsId::SPH_DISCRETIZATION);
    /// \todo partially duplicates stuff from EquationTerm.cpp
    switch (formulation) {
    case DiscretizationEnum::STANDARD:
        struct StandardDiscr {
            INLINE SymmetricTensor operator()(const SymmetricTensor& asi,
                const SymmetricTensor& asj,
                const Float rhoi,
                const Float rhoj) const {
                return asi / sqr(rhoi) + asj / sqr(rhoj);
            }
        };
        derivatives.require(makeAuto<Derivative<StandardDiscr>>(settings));
        break;
    case DiscretizationEnum::BENZ_ASPHAUG:
        struct BenzAsphaugDiscr {
            INLINE SymmetricTensor operator()(const SymmetricTensor& asi,
                const SymmetricTensor& asj,
                const Float rhoi,
                const Float rhoj) const {
                return (asi + asj) / (rhoi * rhoj);
            }
        };
        derivatives.require(makeAuto<Derivative<BenzAsphaugDiscr>>(settings));
        break;
    default:
        NOT_IMPLEMENTED;
    }
}

void StressAV::initialize(IScheduler& scheduler, Storage& storage) {
    ArrayView<SymmetricTensor> as = storage.getValue<SymmetricTensor>(QuantityId::AV_STRESS);
    ArrayView<const TracelessTensor> s = storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ArrayView<const Float> p = storage.getValue<Float>(QuantityId::PRESSURE);

    parallelFor(scheduler, 0, p.size(), [&as, &s, &p](const Size i) {
        // compute total stress tesor
        const SymmetricTensor sigma = SymmetricTensor(s[i]) - p[i] * SymmetricTensor::identity();

        // find eigenvectors and corresponding eigenvalues
        Eigen eigen = eigenDecomposition(sigma);
        AffineMatrix matrix = eigen.vectors;
        Vector sigma_diag = eigen.values;

        // for positive values of diagonal stress, compute artificial stress
        const Vector as_diag = -max(sigma_diag, Vector(0._f));

        // transform back to original coordinates
        as[i] = transform(SymmetricTensor(as_diag, Vector(0._f)), matrix.transpose());
    });
}

void StressAV::finalize(IScheduler& UNUSED(scheduler), Storage& UNUSED(storage)) {}

void StressAV::create(Storage& storage, IMaterial& UNUSED(material)) const {
    Quantity& q = storage.insert<Float>(QuantityId::INTERPARTICLE_SPACING_KERNEL, OrderEnum::ZERO, 0._f);
    ArrayView<Float> wp = q.getValue<Float>();
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < r.size(); ++i) {
        /// Delta p / h is assumed to be constant, so that W(Delta p) is constant
        wp[i] = kernel.value(Vector(r[i][H], 0._f, 0._f), r[i][H]);
    }
    storage.insert<SymmetricTensor>(QuantityId::AV_STRESS, OrderEnum::ZERO, SymmetricTensor::null());
}

NAMESPACE_SPH_END
