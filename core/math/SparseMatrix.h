#pragma once

/// \file SparseMatrix.h
/// \brief Sparse matrix with utility functions (a thin wrapper over Eigen3 implementation)
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/containers/Array.h"
#include "objects/wrappers/AutoPtr.h"
#include "objects/wrappers/Expected.h"
#include "objects/wrappers/PropagateConst.h"

NAMESPACE_SPH_BEGIN

/// \brief Sparse representation of matrix of arbitrary dimension
class SparseMatrix {
private:
    class Impl;
    PropagateConst<AutoPtr<Impl>> impl;

public:
    SparseMatrix();

    /// Constructs square n x m empty matrix
    SparseMatrix(const Size rows, const Size cols);

    ~SparseMatrix();

    /// Changes the size of the matrix, removing all previous entries.
    void resize(const Size rows, const Size cols);

    /// \brief Adds a values to given element of the matrix.
    ///
    /// If there is already a nonzero element, both values are summed up.
    void insert(const Size i, const Size j, const Float value);

    /// Solvers of sparse systems
    enum class Solver {
        /// LU factorization, precise but very slow for large problems
        LU,

        /// Conjugate gradient, approximative (iterative) solver, can only be used for symmetric
        /// positive-definite matrices!
        CG,

        /// Least-square conjugate gradient, can be used for any matrix
        LSCG,

        /// Stabilized bi-conjugate gradient method, can be used for any square matrix
        BICGSTAB,
    };

    /// Solvers an equation Ax = b, where A is the sparse matrix and b is given array of values.
    /// \param values Array of values b. The size of the array must be the same as the size of the matrix.
    /// \param solver Solver used to solve the system of equations
    /// \param tolerance Threshold used by the stopping criterion, only used by iterative solvers.
    /// \return Solution vector or error message
    Expected<Array<Float>> solve(const Array<Float>& values, const Solver solver, const Float tolerance = 0.);
};

NAMESPACE_SPH_END
