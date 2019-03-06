#include "sph/solvers/ElasticDeformationSolver.h"
#include "math/Quat.h"
#include "objects/finders/NeighbourFinder.h"
#include "quantities/IMaterial.h"
#include "quantities/Quantity.h"
#include "sph/equations/Derivative.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN


/// \brief Extracts rotation from a generic deformation matrix.
///
/// Following Muller et al. (2016).
/// \param a Deformation matrix
/// \param r Initial estimate of the rotation matrix
/// \param iterationCnt Number of iterations, higher value means more precise result, but slower computation
static AffineMatrix extractRotation(const AffineMatrix& a,
    const AffineMatrix& r0,
    const Size iterationCnt = 3) {
    const Vector a1 = a.column(0);
    const Vector a2 = a.column(1);
    const Vector a3 = a.column(2);

    AffineMatrix r = r0;
    for (Size i = 0; i < iterationCnt; ++i) {
        const Vector r1 = r.column(0);
        const Vector r2 = r.column(1);
        const Vector r3 = r.column(2);

        const Vector omega = (cross(r1, a1) + cross(r2, a2) + cross(r3, a3)) /
                             (abs(dot(r1, a1) + dot(r2, a2) + dot(r3, a3)) + EPS);
        Vector dir;
        Float angle;
        tieToTuple(dir, angle) = getNormalizedWithLength(omega);
        r = r * AffineMatrix::rotateAxis(dir, angle);
    }
    return r;
}

ElasticDeformationSolver::ElasticDeformationSolver(IScheduler& scheduler, const RunSettings& settings)
    : scheduler(scheduler) {
    kernel = Factory::getKernel<3>(settings);
}

void ElasticDeformationSolver::integrate(Storage& storage, Statistics& stats) {
    (void)stats;

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<const Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
    /*ArrayView<const SymmetricTensor> C =
        storage.getValue<SymmetricTensor>(QuantityId::STRAIN_RATE_CORRECTION_TENSOR);*/

    finder->build(scheduler, r);

    const Float maxH = r[0][H]; /// \todo

    if (neighsPerParticle.empty()) {
        // lazy initialization
        neighsPerParticle.resize(r.size());
        Array<NeighbourRecord> neighs;
        for (Size i = 0; i < r.size(); ++i) {
            const Size neighCnt = finder->findAll(i, maxH * kernel.radius(), neighs);
            neighsPerParticle[i].resize(neighCnt);
            for (NeighbourRecord& n : neighs) {
                neighsPerParticle[i].push(n.index);
            }
        }
    }

    const Float mu = 0._f;
    const Float lambda = 0._f;
    for (Size i = 0; i < r.size(); ++i) {
        Tensor F = Tensor::null();
        for (Size j : neighsPerParticle[i]) {
            F += m[j] / rho[j] * outer(r[j] - r[i], w[i]);
        }
        const AffineMatrix R = extractRotation(convert<AffineMatrix>(F), AffineMatrix::identity());
        (void)R;
        const SymmetricTensor e = symmetrize(F);
        const SymmetricTensor sigma = 2._f * mu * e + lambda * e.trace() * SymmetricTensor::identity();
        (void)sigma;
    }
}

void ElasticDeformationSolver::create(Storage& storage, IMaterial& UNUSED(material)) const {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    finder->build(scheduler, r);

    storage.insert<SymmetricTensor>(
        QuantityId::STRAIN_RATE_CORRECTION_TENSOR, OrderEnum::ZERO, SymmetricTensor::identity());

    CorrectionTensor ct(RunSettings::getDefaults());
    Accumulated accumulated;
    ct.initialize(storage, accumulated);

    Array<Size> neighs;
    Array<Vector> grads;
    for (Size i = 0; i < storage.getParticleCnt(); ++i) {
        ct.evalNeighs(i, neighs, grads);
        // w[i] = kernel.grad(r[i]);
    }
    accumulated.store(storage);
}

NAMESPACE_SPH_END
