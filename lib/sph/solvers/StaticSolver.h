#pragma once

/// \file StaticSolver.h
/// \brief Computes quantities to get equilibrium state
/// \author Pavel Sevecek
/// \date 2016-2017

#include "math/SparseMatrix.h"
#include "objects/wrappers/Outcome.h"
#include "sph/equations/EquationTerm.h"
#include "sph/kernel/KernelFactory.h"
#include "sph/solvers/GenericSolver.h"

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
        results.insert<Float>(QuantityId::PRESSURE);
        results.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        u = input.getValue<Vector>(QuantityId::DISPLACEMENT);
        tie(m, rho) = input.getValues<Float>(QuantityId::MASSES, QuantityId::DENSITY);
        p = results.getValue<Float>(QuantityId::PRESSURE);
        s = results.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);

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

/// Solves for total stress tensor sigma. Equations to be solved cannot be specified at the moment, that would
/// require a lot of extra work and it is not needed at the moment. Will be possibly extended in the future.
class StaticSolver {
private:
    AutoPtr<Abstract::Finder> finder;

    SymmetrizeSmoothingLengths<LutKernel<3>> kernel;

    GenericSolver equationSolver;

    Size boundaryThreshold;

    SparseMatrix matrix;

public:
    /// Constructs the solver.
    /// \param equations Additional forces. The forces can depend on spatial derivatives, but must be
    ///                  independent on both pressure and deviatoric stress. All quantities appearing in these
    ///                  equations are considered parameters of the problem, solver cannot be used to solve
    ///                  other quantities than the total stress tensor.
    StaticSolver(const RunSettings& settings, EquationHolder&& equations)
        : equationSolver(settings, std::move(equations) + makeTerm<DisplacementTerm>()) {
        kernel = Factory::getKernel<3>(settings);
        finder = Factory::getFinder(settings);
        boundaryThreshold = 20;
        // settings.get<int>(RunSettingsId::BOUNDARY_THRESHOLD);
    }

    /// Computed pressure and deviatoric stress are placed into the storage, overriding previously stored
    /// values.
    Outcome solve(Storage& storage, Statistics& stats) {
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
        for (Size i = 0; i < dv.size(); ++i) {
            for (Size j = 0; j < 3; ++j) {
                const Float x = rho[i] * dv[i][j];
                b[3 * i + j] = x;
                b_avg += x;
            }
        }
        b_avg /= dv.size() * 3;

        // get number of neighbours for boundary detection
        ArrayView<const Size> neighCnts = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);

        Array<NeighbourRecord> neighs;
        matrix.resize(r.size() * 3);

        // The equation we are trying to solve is:
        // (\lambda + \mu) \nabla (\nabla \cdot u) + \mu \nabla^2 u + f = 0
        // where \lambda, \mu are Lame's coefficient, u is the displacement vector and f is the external force

        ASSERT(storage.getMaterialCnt() == 1); /// \todo generalize for heterogeneous bodies
        Abstract::Material& material = storage.getMaterial(0);
        const Float lambda = material.getParam<Float>(BodySettingsId::ELASTIC_MODULUS);
        const Float mu = material.getParam<Float>(BodySettingsId::SHEAR_MODULUS);

        // fill the matrix with values
        for (Size i = 0; i < r.size(); ++i) {
            finder->findNeighbours(i, kernel.radius() * r[i][H], neighs, FinderFlags::FIND_ONLY_SMALLER_H);

            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k].index;
                const Vector grad = kernel.grad(r[i], r[j]);
                const Vector dr = r[i] - r[j];
                const Float f = dot(dr, grad) / getSqrLength(dr);
                const Vector dr0 = getNormalized(dr);
                /// \todo check sign
                ASSERT(isReal(f));
                SymmetricTensor lhs =
                    -5._f * (lambda + mu) * outer(dr0, dr0) + (lambda - mu) * SymmetricTensor::identity();
                ASSERT(isReal(lhs));

                SymmetricTensor mij = m[j] / rho[j] * lhs * f;
                SymmetricTensor mji = m[i] / rho[i] * lhs * f;
                for (Size a = 0; a < 3; ++a) {
                    for (Size b = 0; b < 3; ++b) {
                        if (neighCnts[i] < boundaryThreshold) {
                            matrix.insert(3 * i + a, 3 * i + b, b_avg * LARGE);
                        } else {
                            matrix.insert(3 * i + a, 3 * i + b, mij(a, b));
                        }
                        matrix.insert(3 * i + a, 3 * j + b, -mij(a, b));
                        if (neighCnts[j] < boundaryThreshold) {
                            matrix.insert(3 * j + a, 3 * j + b, b_avg * LARGE);
                        } else {
                            matrix.insert(3 * j + a, 3 * j + b, mji(a, b));
                        }
                        matrix.insert(3 * j + a, 3 * i + b, -mji(a, b));
                    }
                }
            }
        }

        // solve the system of equation for displacement
        Expected<Array<Float>> a = matrix.solve(b, SparseMatrix::Solver::LSCG, 0.1f);
        if (!a) {
            // somehow cannot solve the system of equations, report the error
            return a.error();
        }

        // fill the displacement array with computed values
        ArrayView<Vector> u = storage.getValue<Vector>(QuantityId::DISPLACEMENT);
        for (Size i = 0; i < u.size(); ++i) {
            for (Size j = 0; j < 3; ++j) {
                u[i][j] = a.value()[3 * i + j];
            }
        }

        // compute pressure and deviatoric stress from displacement
        equationSolver.integrate(storage, stats);

        /// \todo saving accumulated to highest-order derivatives doesnt work as we can 'extend' quantities by
        /// adding more derivatives. The computation of stress tensor here would be broken by adding Hooke's
        /// law!!

        ASSERT(storage.getQuantity(QuantityId::PRESSURE).getOrderEnum() == OrderEnum::ZERO);
        ASSERT(storage.getQuantity(QuantityId::DEVIATORIC_STRESS).getOrderEnum() == OrderEnum::ZERO);

        return SUCCESS;
    }

    void create(Storage& storage, Abstract::Material& material) {
        ASSERT(storage.getMaterialCnt() == 1);
        equationSolver.create(storage, material);
    }
};

NAMESPACE_SPH_END
