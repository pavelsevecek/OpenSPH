#include "sph/solvers/EquilibriumSolver.h"
#include "gravity/IGravity.h"
#include "objects/finders/NeighborFinder.h"
#include "physics/Eos.h"
#include "sph/Materials.h"
#include "sph/equations/DerivativeHelpers.h"
#include "sph/equations/EquationTerm.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

#ifdef SPH_USE_EIGEN

EquilibriumEnergySolver::EquilibriumEnergySolver(IScheduler& scheduler,
    const RunSettings& settings,
    AutoPtr<IGravity>&& gravity,
    const Size boundaryThreshold)
    : scheduler(scheduler)
    , gravity(std::move(gravity))
    , boundaryThreshold(boundaryThreshold) {
    kernel = Factory::getKernel<3>(settings);
    finder = Factory::getFinder(settings);
}

EquilibriumEnergySolver::~EquilibriumEnergySolver() = default;

Outcome EquilibriumEnergySolver::solve(Storage& storage, Statistics& stats) {
    // compute gravity to use as right-hand side
    gravity->build(scheduler, storage);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    gravity->evalSelfGravity(scheduler, dv, stats);

    ArrayView<Float> m = storage.getValue<Float>(QuantityId::MASS);
    // add centrifugal force
    for (Size i = 0; i < r.size(); ++i) {
        const Float R = sqrt(sqr(r[i][X]) + sqr(r[i][Y]));
        dv[i] += getSqrLength(v[i]) / R * Vector(r[i][X], r[i][Y], 0._f) / R;
    }

    finder->build(scheduler, r);
    Float maxRadius = 0._f;
    for (Size i = 0; i < r.size(); ++i) {
        maxRadius = max(maxRadius, r[i][H]);
    }

    ArrayView<Float> rho = storage.getValue<Float>(QuantityId::DENSITY);

#if 0
    SparseMatrix matrix(3 * r.size(), r.size());
    Array<Float> b(3 * r.size());
    Array<NeighbourRecord> neighs;
    Float lambda = 0._f;
    for (Size i = 0; i < r.size(); ++i) {
        neighs.clear();
        finder->findAll(i, maxRadius * kernel.radius(), neighs);
        /*if (neighs.size() < boundaryThreshold) {
            // boundary -> dirichlet
            for (int k = 0; k < 3; ++k) {
                matrix.insert(3 * i + k, i, 1);
                b[3 * i + k] = 0;
            }
            continue;
        }*/
        const bool isBoundary = neighs.size() < boundaryThreshold;

        Vector Aii(0._f);
        Float weightSum = 0;
        for (const NeighbourRecord& n : neighs) {
            const Size j = n.index;
            const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
            SPH_ASSERT(hbar > EPS, hbar);
            if (i == j || n.distanceSqr >= sqr(kernel.radius() * hbar)) {
                // aren't actual neighbors
                continue;
            }
            const Vector grad = kernel.grad(r[i], r[j]);
            const Float weight = m[i] / (r[i][H] * sqr(rho[i])) * kernel.value(r[i], r[j]);
            Aii += m[j] * grad / sqr(rho[i]);
            const Vector Aij = m[j] * grad / sqr(rho[j]);
            if (!isBoundary) {
                for (int k = 0; k < 3; ++k) {
                    matrix.insert(3 * i + k, j, Aij[k] - lambda * weight);
                }
            } else {
                for (int k = 0; k < 3; ++k) {
                    matrix.insert(3 * i + k, j, -lambda * weight);
                }
            }
            weightSum += weight;
        }
        // add diagonal element and right-hand side
        if (!isBoundary) {
            for (int k = 0; k < 3; ++k) {
                matrix.insert(3 * i + k, i, Aii[k] + weightSum * lambda);
                b[3 * i + k] = dv[i][k];
            }
        } else {
            for (int k = 0; k < 3; ++k) {
                matrix.insert(3 * i + k, i, 1.f + weightSum * lambda);
                b[3 * i + k] = 0._f;
            }
        }
    }
    Expected<Array<Float>> X = matrix.solve(b, SparseMatrix::Solver::LSCG, 1.e-8f);
    if (!X) {
        return X.error();
    }

#else

    SparseMatrix matrix(r.size(), r.size());
    Array<Float> b(r.size());
    Array<NeighborRecord> neighs;

    for (Size i = 0; i < r.size(); ++i) {
        neighs.clear();
        finder->findAll(i, maxRadius * kernel.radius(), neighs);

        Float Aii(0._f);
        Float divDv = 0._f;
        for (const NeighborRecord& n : neighs) {
            const Size j = n.index;
            const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
            SPH_ASSERT(hbar > EPS, hbar);
            if (i == j || n.distanceSqr >= sqr(kernel.radius() * hbar)) {
                // aren't actual neighbors
                continue;
            }
            const Vector grad = kernel.grad(r[i], r[j]);
            const Float lapl = laplacian(1._f, grad, r[i] - r[j]);
            Aii -= m[j] * lapl / sqr(rho[i]);
            const Float Aij = m[j] * lapl / sqr(rho[j]);
            matrix.insert(i, j, Aij);

            divDv -= m[j] / rho[j] * dot(dv[j] - dv[i], grad);
        }

        /*if (neighs.size() < boundaryThreshold) {
            for (const NeighbourRecord& n1 : neighs) {
                const Size j1 = n1.index;
                const Vector mirrorR = 2._f * r[i] - r[j1];
                bool mirrorFound = false;
                for (const NeighbourRecord& n2 : neighs) {
                    const Size j2 = n2.index;
                    if (getSqrLength(r[j2] - mirrorR) < 0.25_f * r[j2][H]) {
                        mirrorFound = true;
                        break;
                    }
                }
                if (!mirrorFound) {
                    const Vector grad = kernel.grad(r[i], mirrorR);
                    const Float lapl = laplacian(1._f, grad, r[i] - mirrorR);
                    Aii -= m[j1] * lapl / sqr(rho[i]);
                }
            }
        }*/


        if (neighs.size() < boundaryThreshold) {
            // boundary -> dirichlet
            Aii += 100._f * m[i] / sqr(rho[i]) * kernel.value(r[i], r[i]) / sqr(r[i][H]);
        }

        // add diagonal element and right-hand side
        matrix.insert(i, i, Aii);
        b[i] = divDv;
    }
    Expected<Array<Float>> X = matrix.solve(b, SparseMatrix::Solver::BICGSTAB);
    if (!X) {
        return makeFailed(X.error());
    }
#endif

    ArrayView<Float> u = storage.getValue<Float>(QuantityId::ENERGY);
    ArrayView<Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
    for (Size matId = 0; matId < storage.getMaterialCnt(); matId++) {
        MaterialView mat = storage.getMaterial(matId);
        EosMaterial& eosMat = dynamic_cast<EosMaterial&>(mat.material());
        const IEos& eos = eosMat.getEos();
        for (Size i : mat.sequence()) {
            p[i] = X.value()[i];
            u[i] = eos.getInternalEnergy(rho[i], p[i]);
        }
    }

    return SUCCESS;
}

#endif

/// Derivative computing components of stress tensor from known displacement vectors.
class DisplacementGradient : public DerivativeTemplate<DisplacementGradient> {
private:
    ArrayView<const Vector> u;
    ArrayView<const Float> m, rho;
    ArrayView<Float> p;
    ArrayView<TracelessTensor> s;

    Float lambda, mu;

public:
    DisplacementGradient(const RunSettings& settings)
        : DerivativeTemplate<DisplacementGradient>(settings) {}

    INLINE void additionalCreate(Accumulated& results) {
        results.insert<Float>(QuantityId::PRESSURE, OrderEnum::ZERO, BufferSource::UNIQUE);
        results.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS, OrderEnum::ZERO, BufferSource::UNIQUE);
    }

    INLINE void additionalInitialize(const Storage& input, Accumulated& results) {
        u = input.getValue<Vector>(QuantityId::DISPLACEMENT);
        tie(m, rho) = input.getValues<Float>(QuantityId::MASS, QuantityId::DENSITY);

        p = results.getBuffer<Float>(QuantityId::PRESSURE, OrderEnum::ZERO);
        s = results.getBuffer<TracelessTensor>(QuantityId::DEVIATORIC_STRESS, OrderEnum::ZERO);

        /// \todo generalize for heterogeneous body
        IMaterial& material = input.getMaterial(0);
        lambda = material.getParam<Float>(BodySettingsId::ELASTIC_MODULUS);
        mu = material.getParam<Float>(BodySettingsId::SHEAR_MODULUS);
    }

    INLINE bool additionalEquals(const DisplacementGradient& UNUSED(other)) const {
        return true;
    }

    template <bool Symmetrize>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        /// \todo determine actual discretization of this equation
        const SymmetricTensor epsilon = symmetricOuter(u[j] - u[i], grad);
        const SymmetricTensor sigma =
            lambda * epsilon.trace() * SymmetricTensor::identity() + 2._f * mu * epsilon;
        const Float tr3 = sigma.trace() / 3._f;
        TracelessTensor ds(sigma - tr3 * SymmetricTensor::identity());
        p[i] += m[j] / rho[j] * tr3;
        s[i] += m[j] / rho[j] * ds;
        if (Symmetrize) {
            p[j] += m[i] / rho[i] * tr3;
            s[j] += m[i] / rho[i] * ds;
        }
    }
};

class DisplacementTerm : public IEquationTerm {
public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require(makeAuto<DisplacementGradient>(settings));
    }

    virtual void initialize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

    virtual void finalize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

    virtual void create(Storage& storage, IMaterial& UNUSED(material)) const override {
        storage.insert<Float>(QuantityId::PRESSURE, OrderEnum::ZERO, 0._f);
        storage.insert<TracelessTensor>(
            QuantityId::DEVIATORIC_STRESS, OrderEnum::ZERO, TracelessTensor::null());
        storage.insert<Vector>(QuantityId::DISPLACEMENT, OrderEnum::ZERO, Vector(0._f));
    }
};

#ifdef SPH_USE_EIGEN

static EquationHolder getEquations(const EquationHolder& additional) {
    return additional + makeTerm<DisplacementTerm>() + makeTerm<ConstSmoothingLength>();
}

EquilibriumStressSolver::EquilibriumStressSolver(IScheduler& scheduler,
    const RunSettings& settings,
    const EquationHolder& equations)
    : scheduler(scheduler)
    , equationSolver(scheduler, settings, getEquations(equations)) {
    kernel = Factory::getKernel<3>(settings);
    finder = Factory::getFinder(settings);
    boundaryThreshold = 18;
    // settings.get<int>(RunSettingsId::BOUNDARY_THRESHOLD);
}

EquilibriumStressSolver::~EquilibriumStressSolver() = default;

Outcome EquilibriumStressSolver::solve(Storage& storage, Statistics& stats) {
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);

    // build the neighbor finding structure
    finder->build(scheduler, r);

    // compute right-hand side of equations by solving equations for acceleration
    storage.zeroHighestDerivatives();
    equationSolver.integrate(storage, stats);

    ArrayView<Float> m, rho;
    tie(m, rho) = storage.getValues<Float>(QuantityId::MASS, QuantityId::DENSITY);
    ArrayView<const Vector> dv = storage.getD2t<Vector>(QuantityId::POSITION);
    Array<Float> b(dv.size() * 3);
    Float b_avg = 0._f;

    // get number of neighbors for boundary detection
    ArrayView<const Size> neighCnts = storage.getValue<Size>(QuantityId::NEIGHBOR_CNT);

    for (Size i = 0; i < dv.size(); ++i) {
        for (Size j = 0; j < 3; ++j) {
            const Float x = -rho[i] * dv[i][j];
            b[3 * i + j] = x;
            b_avg += abs(x);
        }
    }
    b_avg /= dv.size() * 3;


    // The equation we are trying to solve is:
    // (\lambda + \mu) \nabla (\nabla \cdot u) + \mu \nabla^2 u + f = 0
    // where \lambda, \mu are Lame's coefficient, u is the displacement vector and f is the external force

    SPH_ASSERT(storage.getMaterialCnt() == 1); /// \todo generalize for heterogeneous bodies
    IMaterial& material = storage.getMaterial(0);
    const Float lambda = material.getParam<Float>(BodySettingsId::ELASTIC_MODULUS);
    const Float mu = material.getParam<Float>(BodySettingsId::SHEAR_MODULUS);

    // fill the matrix with values
    Array<NeighborRecord> neighs;
    matrix.resize(r.size() * 3, r.size() * 3);
    for (Size i = 0; i < r.size(); ++i) {
        finder->findLowerRank(i, kernel.radius() * r[i][H], neighs);

        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k].index;
            const Vector grad = kernel.grad(r[i], r[j]);
            const Vector dr = r[i] - r[j];
            const Float f = dot(dr, grad) / getSqrLength(dr);
            const Vector dr0 = getNormalized(dr);
            SPH_ASSERT(isReal(f));
            SymmetricTensor lhs = -5._f * (lambda + mu) * symmetricOuter(dr0, dr0) +
                                  (lambda - mu) * SymmetricTensor::identity();
            SPH_ASSERT(isReal(lhs));

            SymmetricTensor mij = m[j] / rho[j] * lhs * f;
            SymmetricTensor mji = m[i] / rho[i] * lhs * f;
            for (Size a = 0; a < 3; ++a) {
                for (Size b = 0; b < 3; ++b) {
                    matrix.insert(3 * i + a, 3 * i + b, mij(a, b));
                    matrix.insert(3 * i + a, 3 * j + b, -mij(a, b));
                    matrix.insert(3 * j + a, 3 * j + b, mji(a, b));
                    matrix.insert(3 * j + a, 3 * i + b, -mji(a, b));
                    /*if (neighCnts[i] < boundaryThreshold) {
                        matrix.insert(3 * j + a, 3 * i + b, b_avg * LARGE);
                        matrix.insert(3 * j + a, 3 * i + b, b_avg * LARGE);
                    }
                    if (neighCnts[j] < boundaryThreshold) {
                        matrix.insert(3 * j + a, 3 * j + b, b_avg * LARGE);
                        matrix.insert(3 * i + a, 3 * j + b, b_avg * LARGE);
                    }*/
                }
            }
        }
    }

    // solve the system of equation for displacement
    Expected<Array<Float>> a = matrix.solve(b, SparseMatrix::Solver::LSCG, 0.1_f);
    if (!a) {
        // somehow cannot solve the system of equations, report the error
        return makeFailed(a.error());
    }

    // fill the displacement array with computed values
    ArrayView<Vector> u = storage.getValue<Vector>(QuantityId::DISPLACEMENT);
    for (Size i = 0; i < u.size(); ++i) {
        if (neighCnts[i] < boundaryThreshold) {
            u[i] = Vector(0._f);
        } else {
            for (Size j = 0; j < 3; ++j) {
                u[i][j] = a.value()[3 * i + j];
            }
        }
    }

    // compute pressure and deviatoric stress from displacement
    equationSolver.integrate(storage, stats);

    // compute internal energy based on pressure (pressure is computed every time step using the equation
    // of state, so our computed values would be overriden)
    ArrayView<Float> p, e;
    tie(p, rho, e) = storage.getValues<Float>(QuantityId::PRESSURE, QuantityId::DENSITY, QuantityId::ENERGY);
    for (Size matId = 0; matId < storage.getMaterialCnt(); ++matId) {
        MaterialView mat = storage.getMaterial(matId);
        EosMaterial& eosMat = dynamic_cast<EosMaterial&>(mat.material());
        for (Size i : mat.sequence()) {
            e[i] = eosMat.getEos().getInternalEnergy(rho[i], p[i]);
            SPH_ASSERT(isReal(e[i])); // && e[i] >= 0._f, e[i]);
        }
    }
    return SUCCESS;
}

void EquilibriumStressSolver::create(Storage& storage, IMaterial& material) {
    SPH_ASSERT(storage.getMaterialCnt() == 1);
    equationSolver.create(storage, material);
}

#endif

NAMESPACE_SPH_END
