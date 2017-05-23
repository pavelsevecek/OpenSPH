#pragma once

/// \file Matrix.h
/// \brief 3x3 matrix
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017
/// \todo move to geometry? or move geometry here?

#include "geometry/Vector.h"

NAMESPACE_SPH_BEGIN

class Matrix {
private:
    Vector v[3]; // rows

public:
    Matrix() = default;

    /// Construct the matrix from vectors as rows
    Matrix(const Vector& v1, const Vector& v2, const Vector& v3)
        : v{ v1, v2, v3 } {}

    /// \param i Row index
    /// \param j Column index
    INLINE Float& operator()(const Size i, const Size j) {
        return v[i][j];
    }

    INLINE Float operator()(const Size i, const Size j) const {
        return v[i][j];
    }

    INLINE Vector column(const Size idx) const {
        return Vector(v[0][idx], v[1][idx], v[2][idx]);
    }

    INLINE Vector row(const Size idx) const {
        return v[idx];
    }

    Matrix transpose() const {
        return Matrix(column(0), column(1), column(2));
    }

    Matrix inverse() const {
        const Float det = v[0][0] * (v[1][1] * v[2][2] - v[2][1] * v[1][2]) -
                          v[0][1] * (v[1][0] * v[2][2] - v[1][2] * v[2][0]) +
                          v[0][2] * (v[1][0] * v[2][1] - v[1][1] * v[2][0]);

        ASSERT(det != 0._f);
        const Float invdet = 1 / det;

        Matrix inv;
        inv(0, 0) = (v[1][1] * v[2][2] - v[2][1] * v[1][2]) * invdet;
        inv(0, 1) = (v[0][2] * v[2][1] - v[0][1] * v[2][2]) * invdet;
        inv(0, 2) = (v[0][1] * v[1][2] - v[0][2] * v[1][1]) * invdet;
        inv(1, 0) = (v[1][2] * v[2][0] - v[1][0] * v[2][2]) * invdet;
        inv(1, 1) = (v[0][0] * v[2][2] - v[0][2] * v[2][0]) * invdet;
        inv(1, 2) = (v[1][0] * v[0][2] - v[0][0] * v[1][2]) * invdet;
        inv(2, 0) = (v[1][0] * v[2][1] - v[2][0] * v[1][1]) * invdet;
        inv(2, 1) = (v[2][0] * v[0][1] - v[0][0] * v[2][1]) * invdet;
        inv(2, 2) = (v[0][0] * v[1][1] - v[1][0] * v[0][1]) * invdet;
        return inv;
    }
};


NAMESPACE_SPH_END
