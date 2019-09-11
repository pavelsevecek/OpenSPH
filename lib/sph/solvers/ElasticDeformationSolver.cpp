#include "sph/solvers/ElasticDeformationSolver.h"
#include "math/Quat.h"
#include "objects/finders/NeighbourFinder.h"
#include "quantities/IMaterial.h"
#include "quantities/Quantity.h"
#include "sph/equations/Derivative.h"
#include "system/Factory.h"
#include "thread/Scheduler.h"

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
        if (omega != Vector(0._f)) {
            Vector dir;
            Float angle;
            tieToTuple(dir, angle) = getNormalizedWithLength(omega);
            r = AffineMatrix::rotateAxis(dir, angle) * r;
            ASSERT(isReal(r.row(0)) && isReal(r.row(1)) && isReal(r.row(2)));
        }
    }

    return r;
}

ElasticDeformationSolver::ElasticDeformationSolver(IScheduler& scheduler,
    const RunSettings& settings,
    AutoPtr<IBoundaryCondition>&& bc)
    : scheduler(scheduler)
    , bc(std::move(bc)) {
    kernel = Factory::getKernel<3>(settings);
    finder = Factory::getFinder(settings);

    gravity = settings.get<Vector>(RunSettingsId::FRAME_CONSTANT_ACCELERATION);
}

void ElasticDeformationSolver::integrate(Storage& storage, Statistics& UNUSED(stats)) {
    bc->initialize(storage);
    bc->finalize(storage);

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<const Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
    ArrayView<Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
    ArrayView<TracelessTensor> S = storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ArrayView<SymmetricTensor> C =
        storage.getValue<SymmetricTensor>(QuantityId::STRAIN_RATE_CORRECTION_TENSOR);
    ArrayView<Vector> dv = storage.getD2t<Vector>(QuantityId::POSITION);
    ArrayView<Vector> alpha = storage.getValue<Vector>(QuantityId::PHASE_ANGLE);

    if (neighsPerParticle.empty()) {
        // lazy initialization
        r0.resize(r.size());
        V0.resize(r.size());

        (void)scheduler;
        finder->build(SEQUENTIAL, r);
        const Float maxH = r[0][H]; /// \todo
        neighsPerParticle.resize(r.size());
        Array<NeighbourRecord> neighs;
        for (Size i = 0; i < r.size(); ++i) {
            r0[i] = r[i];
            V0[i] = m[i] / rho[i];

            finder->findAll(i, maxH * kernel.radius(), neighs);

            ASSERT(C[i] == SymmetricTensor::identity());
            C[i] = SymmetricTensor::null();
            for (NeighbourRecord& n : neighs) {
                const Size j = n.index;
                const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
                if (n.distanceSqr < sqr(kernel.radius() * hbar)) {
                    neighsPerParticle[i].push(j);

                    C[i] += m[j] / rho[j] * symmetricOuter(r[j] - r[i], kernel.grad(r[i], r[j]));
                }
            }
            if (C[i] == SymmetricTensor::null()) {
                C[i] = SymmetricTensor::identity();
            } else {
                C[i] = C[i].inverse();
            }
        }
        R.resize(r.size());
        R.fill(AffineMatrix::identity());

#ifdef SPH_DEBUG
        for (Size i = 0; i < r.size(); ++i) {
            for (Size j : neighsPerParticle[i]) {
                ASSERT(std::find(neighsPerParticle[j].begin(), neighsPerParticle[j].end(), i) !=
                       neighsPerParticle[j].end());
            }
        }
#endif
    }

    for (Size matId = 0; matId < storage.getMaterialCnt(); ++matId) {
        MaterialView mat = storage.getMaterial(matId);
        const Float mu = mat->getParam<Float>(BodySettingsId::SHEAR_MODULUS);
        const Float lambda = mat->getParam<Float>(BodySettingsId::ELASTIC_MODULUS);

        IndexSequence seq = mat.sequence();
        parallelFor(scheduler, *seq.begin(), *seq.end(), [&](const Size i) {
            // compute preliminar deformation gradient (Eq. 3)
            Tensor F = Tensor::null();
            for (Size j : neighsPerParticle[i]) {
                const Vector w = C[i] * kernel.grad(r0[i], r0[j]);
                F += V0[j] * outer(r[j] - r[i], w);
            }

            // extract rotation
            R[i] = extractRotation(convert<AffineMatrix>(F), R[i]);
            const Quat q(R[i]);
            if (abs(q.angle()) > EPS) {
                alpha[i] = q.axis() * q.angle() * RAD_TO_DEG;
            } else {
                alpha[i] = Vector(0._f);
            }

            // compute corotated deformation gradient (Eq. 5)
            Tensor F_star = Tensor::identity();
            for (Size j : neighsPerParticle[i]) {
                const Vector w_star = R[i] * (C[i] * kernel.grad(r0[i], r0[j]));
                F_star += V0[j] * outer(r[j] - r[i] - R[i] * (r0[j] - r0[i]), w_star);
            }

            // compute stress tensor
            const SymmetricTensor e = symmetrize(F_star) - SymmetricTensor::identity();
            const SymmetricTensor sigma = 2._f * mu * e + lambda * e.trace() * SymmetricTensor::identity();
            p[i] = -sigma.trace() / 3._f;
            S[i] = TracelessTensor(sigma + p[i] * SymmetricTensor::identity());
        });

        /// \todo needs to be generalized if particles with different materials interact
        parallelFor(scheduler, *seq.begin(), *seq.end(), [&](const Size i) {
            for (Size j : neighsPerParticle[i]) {
                const Vector w = kernel.grad(r0[i], r0[j]);
                const Vector wi_star = R[i] * (C[i] * w);
                const Vector wj_star = R[j] * (C[j] * w);
                const Vector f =
                    V0[i] * V0[j] * (-p[i] * wi_star - p[j] * wj_star + S[i] * wi_star + S[j] * wj_star);
                dv[i] += f / m[i];
            }

            dv[i] += gravity;
        });
    }
}

void ElasticDeformationSolver::create(Storage& storage, IMaterial& UNUSED(material)) const {
    storage.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS, OrderEnum::ZERO, TracelessTensor::null());
    storage.insert<SymmetricTensor>(
        QuantityId::STRAIN_RATE_CORRECTION_TENSOR, OrderEnum::ZERO, SymmetricTensor::identity());
    storage.insert<Vector>(QuantityId::PHASE_ANGLE, OrderEnum::ZERO, Vector(0._f));
}

NAMESPACE_SPH_END
