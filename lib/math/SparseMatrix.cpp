#include "math/SparseMatrix.h"

/// Disable some warning to compile Eigen with gcc 7.1
#ifdef SPH_GCC
#if __GNUC__ >= 7
#pragma GCC diagnostic ignored "-Wint-in-bool-context"
#endif
#endif

#ifdef SPH_USE_EIGEN
#include <Eigen/IterativeLinearSolvers>
#include <Eigen/SparseLU>
#endif

NAMESPACE_SPH_BEGIN

#ifdef SPH_USE_EIGEN

class SparseMatrix::Impl {
private:
    Eigen::SparseMatrix<Float> matrix;
    Array<Eigen::Triplet<Float>> triplets;

    using SparseVector = Eigen::Matrix<Float, Eigen::Dynamic, 1>;

public:
    Impl(const Size rows, const Size cols)
        : matrix(rows, cols) {}

    void insert(const Size i, const Size j, const Float value) {
        ASSERT(i < matrix.innerSize() && j < matrix.innerSize(), i, j);
        triplets.push(Eigen::Triplet<Float>(i, j, value));
    }

    Expected<Array<Float>> solve(const Array<Float>& values,
        const SparseMatrix::Solver solver,
        const Float tolerance) {
        ASSERT(values.size() == matrix.innerSize());
        // this is takes straigh from Eigen documentation
        // (http://eigen.tuxfamily.org/dox-devel/group__TopicSparseSystems.html)

        matrix.setFromTriplets(triplets.begin(), triplets.end());
        /// \todo experiment with different solvers

        SparseVector b;
        b.resize(values.size());
        for (Size i = 0; i < values.size(); ++i) {
            b(i) = values[i];
        }

        Expected<SparseVector> a;
        switch (solver) {
        case SparseMatrix::Solver::LU: {
            Eigen::SparseLU<Eigen::SparseMatrix<Float>, Eigen::COLAMDOrdering<int>> solver;
            a = solveImpl(solver, b);
            break;
        }
        case SparseMatrix::Solver::CG: {
#ifdef SPH_DEBUG
            // check that the matrix is symmetric
            {
                const Size n = matrix.innerSize();
                for (Size i = 0; i < n; ++i) {
                    for (Size j = i; j < n; ++j) {
                        const Float mij = matrix.coeff(i, j);
                        const Float mji = matrix.coeff(j, i);
                        ASSERT(almostEqual(mij, mji), mij, mji);
                    }
                }
            }
#endif
            Eigen::ConjugateGradient<Eigen::SparseMatrix<Float>, Eigen::Lower | Eigen::Upper> solver;
            if (tolerance > 0._f) {
                solver.setTolerance(tolerance);
            }
            a = solveImpl(solver, b);
            break;
        }
        case SparseMatrix::Solver::LSCG: {
            Eigen::LeastSquaresConjugateGradient<Eigen::SparseMatrix<Float>> solver;
            if (tolerance > 0._f) {
                solver.setTolerance(tolerance);
            }
            a = solveImpl(solver, b);
            break;
        }
        case SparseMatrix::Solver::BICGSTAB: {
            Eigen::BiCGSTAB<Eigen::SparseMatrix<Float>> solver;
            if (tolerance > 0._f) {
                solver.setTolerance(tolerance);
            }
            a = solveImpl(solver, b);
            break;
        }
        default:
            NOT_IMPLEMENTED;
        }

        if (!a) {
            return makeUnexpected<Array<Float>>(a.error());
        }
        Array<Float> result;
        result.resize(matrix.outerSize());
        for (Size i = 0; i < result.size(); ++i) {
            result[i] = a.value()(i);
        }
        return result;
    }

private:
    template <typename TSolver>
    Expected<SparseVector> solveImpl(TSolver& solver, const SparseVector& b) {
        solver.compute(matrix);
        if (solver.info() != Eigen::Success) {
            return makeUnexpected<SparseVector>("Decomposition of matrix failed");
        }
        SparseVector a;
        a = solver.solve(b);
        if (solver.info() != Eigen::Success) {
            return makeUnexpected<SparseVector>("Equations cannot be solved");
        }
        return a;
    }
};

SparseMatrix::SparseMatrix() = default;

SparseMatrix::SparseMatrix(const Size rows, const Size cols)
    : impl(makeAuto<Impl>(rows, cols)) {}

SparseMatrix::~SparseMatrix() = default;

void SparseMatrix::resize(const Size rows, const Size cols) {
    impl = makeAuto<Impl>(rows, cols);
}

void SparseMatrix::insert(const Size i, const Size j, const Float value) {
    impl->insert(i, j, value);
}

Expected<Array<Float>> SparseMatrix::solve(const Array<Float>& values,
    const Solver solver,
    const Float tolerance) {
    return impl->solve(values, solver, tolerance);
}

#endif

NAMESPACE_SPH_END
