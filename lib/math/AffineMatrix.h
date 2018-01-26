#pragma once

/// \file AffineMatrix.h
/// \brief Three-dimensional affine matrix
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/geometry/Vector.h"

NAMESPACE_SPH_BEGIN

/// \todo somewhat duplicate of Tensor, but many function differ slightly due to translation, different
/// asserts, etc. Probably not worth having common parent.
class AffineMatrix {
private:
    Vector v[3]; // rows

public:
    AffineMatrix() = default;

    /// \brief Construct the matrix from vectors as rows.
    ///
    /// Translation is stored as 4th components of vectors
    AffineMatrix(const Vector& v1, const Vector& v2, const Vector& v3)
        : v{ v1, v2, v3 } {}

    /// \param i Row index
    /// \param j Column index
    INLINE Float& operator()(const Size i, const Size j) {
        ASSERT(i < 3 && j < 4, i, j);
        return v[i][j];
    }

    INLINE Float operator()(const Size i, const Size j) const {
        ASSERT(i < 3 && j < 4, i, j);
        return v[i][j];
    }

    INLINE Vector column(const Size idx) const {
        ASSERT(idx < 4, idx);
        return Vector(v[0][idx], v[1][idx], v[2][idx]);
    }

    INLINE Vector row(const Size idx) const {
        ASSERT(idx < 3, idx);
        return v[idx];
    }

    INLINE Vector translation() const {
        return Vector(v[0][3], v[1][3], v[2][3]);
    }

    INLINE AffineMatrix& removeTranslation() {
        v[0][3] = v[1][3] = v[2][3] = 0._f;
        return *this;
    }

    INLINE AffineMatrix& translate(const Vector& t) {
        v[0][3] += t[0];
        v[1][3] += t[1];
        v[2][3] += t[2];
        return *this;
    }

    /// \brief Returns the transposed matrix.
    ///
    /// Translation vector is copied into the transposed matrix, so that double-transposed matrix is equal to
    /// the original matrix.
    INLINE AffineMatrix transpose() const {
        AffineMatrix transposed(column(0), column(1), column(2));
        transposed(0, 3) = v[0][3];
        transposed(1, 3) = v[1][3];
        transposed(2, 3) = v[2][3];
        return transposed;
    }

    /// \brief Computes determinant of the matrix.
    ///
    /// The translation is ignored, as determinant is defined for square matrices
    INLINE Float determinant() const {
        return v[0][0] * (v[1][1] * v[2][2] - v[2][1] * v[1][2]) -
               v[0][1] * (v[1][0] * v[2][2] - v[1][2] * v[2][0]) +
               v[0][2] * (v[1][0] * v[2][1] - v[1][1] * v[2][0]);
    }

    AffineMatrix inverse() const {
        const Float det = this->determinant();
        ASSERT(det != 0._f);

        // from https://stackoverflow.com/questions/1148309/inverting-a-4x4-matrix
        AffineMatrix inv;
        inv(0, 0) = v[1][1] * v[2][2] - v[2][1] * v[1][2];
        inv(1, 0) = -v[1][0] * v[2][2] + v[2][0] * v[1][2];
        inv(2, 0) = v[1][0] * v[2][1] - v[2][0] * v[1][1];
        inv(0, 1) = -v[0][1] * v[2][2] + v[2][1] * v[0][2];
        inv(1, 1) = v[0][0] * v[2][2] - v[2][0] * v[0][2];
        inv(2, 1) = -v[0][0] * v[2][1] + v[2][0] * v[0][1];
        inv(0, 2) = v[0][1] * v[1][2] - v[1][1] * v[0][2];
        inv(1, 2) = -v[0][0] * v[1][2] + v[1][0] * v[0][2];
        inv(2, 2) = v[0][0] * v[1][1] - v[1][0] * v[0][1];
        inv(0, 3) = -v[0][1] * v[1][2] * v[2][3] + v[0][1] * v[1][3] * v[2][2] + v[1][1] * v[0][2] * v[2][3] -
                    v[1][1] * v[0][3] * v[2][2] - v[2][1] * v[0][2] * v[1][3] + v[2][1] * v[0][3] * v[1][2];
        inv(1, 3) = v[0][0] * v[1][2] * v[2][3] - v[0][0] * v[1][3] * v[2][2] - v[1][0] * v[0][2] * v[2][3] +
                    v[1][0] * v[0][3] * v[2][2] + v[2][0] * v[0][2] * v[1][3] - v[2][0] * v[0][3] * v[1][2];
        inv(2, 3) = -v[0][0] * v[1][1] * v[2][3] + v[0][0] * v[1][3] * v[2][1] + v[1][0] * v[0][1] * v[2][3] -
                    v[1][0] * v[0][3] * v[2][1] - v[2][0] * v[0][1] * v[1][3] + v[2][0] * v[0][3] * v[1][1];

        return inv / det;
    }

    bool isOrthogonal() const {
        for (Size i = 0; i < 3; ++i) {
            for (Size j = 0; j < 3; ++j) {
                const Float x = dot(v[i], v[j]);
                if (!almostEqual(x, i == j, 1.e-6_f)) {
                    return false;
                }
            }
        }
        return true;
    }

    bool isIsotropic() const {
        return v[0][0] == v[1][1] && v[0][0] == v[2][2] && v[0][1] == 0._f && v[0][2] == 0._f &&
               v[1][2] == 0._f;
    }

    static AffineMatrix null() {
        return AffineMatrix(Vector(0._f), Vector(0._f), Vector(0._f));
    }

    static AffineMatrix identity() {
        return AffineMatrix(Vector(1._f, 0._f, 0._f), Vector(0._f, 1._f, 0._f), Vector(0._f, 0._f, 1._f));
    }

    static AffineMatrix scale(const Vector& scaling) {
        return AffineMatrix(
            Vector(scaling[X], 0._f, 0._f), Vector(0._f, scaling[Y], 0._f), Vector(0._f, 0._f, scaling[Z]));
    }

    static AffineMatrix rotateX(const Float angle) {
        const Float c = cos(angle);
        const Float s = sin(angle);
        return AffineMatrix(Vector(1._f, 0._f, 0._f), Vector(0._f, c, -s), Vector(0._f, s, c));
    }

    static AffineMatrix rotateY(const Float angle) {
        const Float c = cos(angle);
        const Float s = sin(angle);
        return AffineMatrix(Vector(c, 0._f, s), Vector(0._f, 1._f, 0._f), Vector(-s, 0._f, c));
    }

    static AffineMatrix rotateZ(const Float angle) {
        const Float c = cos(angle);
        const Float s = sin(angle);
        return AffineMatrix(Vector(c, -s, 0._f), Vector(s, c, 0._f), Vector(0._f, 0._f, 1._f));
    }

    static AffineMatrix rotateAxis(const Vector& axis, const Float angle) {
        ASSERT(almostEqual(getSqrLength(axis), 1._f), getSqrLength(axis));
        const Float u = axis[0];
        const Float v = axis[1];
        const Float w = axis[2];
        const Float s = sin(angle);
        const Float c = cos(angle);
        return {
            Vector(u * u + (v * v + w * w) * c, u * v * (1 - c) - w * s, u * w * (1 - c) + v * s),
            Vector(u * v * (1 - c) + w * s, v * v + (u * u + w * w) * c, v * w * (1 - c) - u * s),
            Vector(u * w * (1 - c) - v * s, v * w * (1 - c) + u * s, w * w + (u * u + v * v) * c),
        };
    }

    static AffineMatrix crossProductOperator(const Vector& a) {
        return {
            Vector(0._f, -a[Z], a[Y]),
            Vector(a[Z], 0._f, -a[X]),
            Vector(-a[Y], a[X], 0._f),
        };
    }

    INLINE AffineMatrix operator+(const AffineMatrix& other) const {
        return AffineMatrix(v[0] + other.v[0], v[1] + other.v[1], v[2] + other.v[2]);
    }

    INLINE AffineMatrix operator-(const AffineMatrix& other) const {
        return AffineMatrix(v[0] - other.v[0], v[1] - other.v[1], v[2] - other.v[2]);
    }

    /// Matrix multiplication
    INLINE AffineMatrix operator*(const AffineMatrix& other) const {
        AffineMatrix result = AffineMatrix::identity();
        for (Size i = 0; i < 3; ++i) {
            for (Size j = 0; j < 3; ++j) {
                result(i, j) = dot(this->row(i), other.column(j));
            }
        }
        // add translation part
        Vector t(0._f);
        const Vector lhs = this->translation();
        const Vector rhs = other.translation();
        for (Size i = 0; i < 3; ++i) {
            t[i] = dot(this->row(i), rhs) + lhs[i];
        }
        result.translate(t);
        return result;
    }

    INLINE Vector operator*(const Vector& u) const {
        return Vector(dot(v[0], u) + v[0][3], dot(v[1], u) + v[1][3], dot(v[2], u) + v[2][3]);
    }

    INLINE friend AffineMatrix operator*(const AffineMatrix& t, const Float v) {
        return AffineMatrix(t.row(0) * v, t.row(1) * v, t.row(2) * v);
    }

    INLINE friend AffineMatrix operator*(const Float v, const AffineMatrix& t) {
        return t * v;
    }

    INLINE friend AffineMatrix operator/(const AffineMatrix& t, const Float v) {
        ASSERT(v != 0._f);
        return AffineMatrix(t.row(0) / v, t.row(1) / v, t.row(2) / v);
    }


    INLINE AffineMatrix& operator+=(const AffineMatrix& other) {
        v[0] += other.v[0];
        v[1] += other.v[1];
        v[2] += other.v[2];
        return *this;
    }

    INLINE AffineMatrix& operator-=(const AffineMatrix& other) {
        v[0] -= other.v[0];
        v[1] -= other.v[1];
        v[2] -= other.v[2];
        return *this;
    }

    /// \todo test
    INLINE AffineMatrix& operator*=(const Float value) {
        v[0] *= value;
        v[1] *= value;
        v[2] *= value;
        return *this;
    }

    INLINE AffineMatrix& operator/=(const Float value) {
        ASSERT(value != 0._f);
        v[0] /= value;
        v[1] /= value;
        v[2] /= value;
        return *this;
    }

    INLINE bool operator==(const AffineMatrix& other) const {
        // vectors check only first 3 components, can be optimized
        return v[0] == other.v[0] && v[0][3] == other.v[0][3] && //
               v[1] == other.v[1] && v[1][3] == other.v[1][3] && //
               v[2] == other.v[2] && v[2][3] == other.v[2][3];
    }

    INLINE bool operator!=(const AffineMatrix& other) const {
        return !(*this == other);
    }

    friend std::ostream& operator<<(std::ostream& stream, const AffineMatrix& t) {
        stream << std::setprecision(PRECISION);
        for (Size i = 0; i < 3; ++i) {
            for (Size j = 0; j < 4; ++j) {
                stream << std::setw(20) << t(i, j);
            }
            stream << std::endl;
        }
        return stream;
    }
};

INLINE bool almostEqual(const AffineMatrix& m1, const AffineMatrix& m2, const Float eps = EPS) {
    for (Size i = 0; i < 4; ++i) {
        if (!almostEqual(m1.column(i), m2.column(i), eps)) {
            return false;
        }
    }
    return true;
}


NAMESPACE_SPH_END
