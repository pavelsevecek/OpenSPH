#include "sph/solvers/StaticSolver.h"
#include "physics/Eos.h"
#include "sph/equations/EquationTerm.h"
#include "sph/kernel/KernelFactory.h"

NAMESPACE_SPH_BEGIN

/// Derivative computing components of stress tensor from known displacement vectors.
class DisplacementGradient : public DerivativeTemplate<DisplacementGradient> {
private:
    ArrayView<const Vector> u;
    ArrayView<const Float> m, rho;
    ArrayView<Float> p;
    ArrayView<TracelessTensor> s;

    Float lambda, mu;

public:
    virtual void create(Accumulated& results) override {
        results.insert<Float>(QuantityId::PRESSURE, OrderEnum::ZERO);
        results.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS, OrderEnum::ZERO);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        u = input.getValue<Vector>(QuantityId::DISPLACEMENT);
        tie(m, rho) = input.getValues<Float>(QuantityId::MASSES, QuantityId::DENSITY);

        p = results.getBuffer<Float>(QuantityId::PRESSURE, OrderEnum::ZERO);
        s = results.getBuffer<TracelessTensor>(QuantityId::DEVIATORIC_STRESS, OrderEnum::ZERO);

        /// \todo generalize for heterogeneous body
        Abstract::Material& material = input.getMaterial(0);
        lambda = material.getParam<Float>(BodySettingsId::ELASTIC_MODULUS);
        mu = material.getParam<Float>(BodySettingsId::SHEAR_MODULUS);
    }

    template <bool Symmetrize>
    INLINE void eval(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
        ASSERT(neighs.size() == grads.size());
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];
            /// \todo determine actual discretization of this equation
            const SymmetricTensor epsilon = outer(u[j] - u[i], grads[k]);
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
    }
};

class DisplacementTerm : public Abstract::EquationTerm {
public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<DisplacementGradient>(settings);
    }

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& UNUSED(storage)) override {}

    virtual void create(Storage& storage, Abstract::Material& UNUSED(material)) const override {
        storage.insert<Float>(QuantityId::PRESSURE, OrderEnum::ZERO, 0._f);
        storage.insert<TracelessTensor>(
            QuantityId::DEVIATORIC_STRESS, OrderEnum::ZERO, TracelessTensor::null());
        storage.insert<Vector>(QuantityId::DISPLACEMENT, OrderEnum::ZERO, Vector(0._f));
    }
};

#ifdef SPH_USE_EIGEN

StaticSolver::StaticSolver(const RunSettings& settings, const EquationHolder& equations)
    : equationSolver(settings, equations + makeTerm<DisplacementTerm>() + makeTerm<ConstSmoothingLength>()) {
    kernel = Factory::getKernel<3>(settings);
    finder = Factory::getFinder(settings);
    boundaryThreshold = 18;
    // settings.get<int>(RunSettingsId::BOUNDARY_THRESHOLD);
}


Outcome StaticSolver::solve(Storage& storage, Statistics& stats) {
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);

    // build the neighbour finding structure
    finder->build(r);

    // compute right-hand side of equations by solving equations for acceleration
    storage.init();
    equationSolver.integrate(storage, stats);

    ArrayView<Float> m, rho;
    tie(m, rho) = storage.getValues<Float>(QuantityId::MASSES, QuantityId::DENSITY);
    ArrayView<const Vector> dv = storage.getD2t<Vector>(QuantityId::POSITIONS);
    Array<Float> b(dv.size() * 3);
    Float b_avg = 0._f;

    // get number of neighbours for boundary detection
    ArrayView<const Size> neighCnts = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);

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

    ASSERT(storage.getMaterialCnt() == 1); /// \todo generalize for heterogeneous bodies
    Abstract::Material& material = storage.getMaterial(0);
    const Float lambda = material.getParam<Float>(BodySettingsId::ELASTIC_MODULUS);
    const Float mu = material.getParam<Float>(BodySettingsId::SHEAR_MODULUS);

    // fill the matrix with values
    Array<NeighbourRecord> neighs;
    matrix.resize(r.size() * 3);
    for (Size i = 0; i < r.size(); ++i) {
        finder->findNeighbours(i, kernel.radius() * r[i][H], neighs, FinderFlags::FIND_ONLY_SMALLER_H);

        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k].index;
            const Vector grad = kernel.grad(r[i], r[j]);
            const Vector dr = r[i] - r[j];
            const Float f = dot(dr, grad) / getSqrLength(dr);
            const Vector dr0 = getNormalized(dr);
            ASSERT(isReal(f));
            SymmetricTensor lhs =
                -5._f * (lambda + mu) * outer(dr0, dr0) + (lambda - mu) * SymmetricTensor::identity();
            ASSERT(isReal(lhs));

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
        return a.error();
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
            ASSERT(isReal(e[i])); // && e[i] >= 0._f, e[i]);
        }
    }
    return SUCCESS;
}

void StaticSolver::create(Storage& storage, Abstract::Material& material) {
    ASSERT(storage.getMaterialCnt() == 1);
    equationSolver.create(storage, material);
}

#endif

NAMESPACE_SPH_END
