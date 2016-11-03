#pragma once

/// Basic tensor algebra
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

/// \todo UNFINISHED!!

#include "geometry/Vector.h"

NAMESPACE_SPH_BEGIN

class Matrix : public Object {
private:
    Vector rows[3];

public:
    Matrix() = default;

    Matrix(const Matrix& other) {
        rows[0] = other.rows[0];
        rows[1] = other.rows[1];
        rows[2] = other.rows[2];
    }

    Matrix(Matrix&& other) {
        rows[0] = std::move(other.rows[0]);
        rows[1] = std::move(other.rows[1]);
        rows[2] = std::move(other.rows[2]);
    }

    Matrix(const Vector& v1, const Vector& v2, const Vector& v3) {
        rows[0] = v1;
        rows[1] = v2;
        rows[2] = v3;
    }


    const double* operator[](uint1 idx) const;
    double* operator[](uint1 idx);

    static Matrix Identity();
    static Matrix Null();
    Matrix Transpose() const;
    Matrix Inverse() const;
    Matrix RemoveTranslation() const;
    static Matrix RotateX(double a);
    static Matrix RotateY(double a);
    static Matrix RotateZ(double a);
    static Matrix RotateEuler(double a, double b, double c);
    static Matrix RotateAxis(const TVector& axis, double a);
    static Matrix Translate(const TVector& t);
    static Matrix Scale(double x, double y, double z);
    static Matrix Scale(double s);
    static Matrix TRS(const TVector& t, const TVector& r, const TVector& s);
    void ToLog(TLog* log = &STD_OUT) const;
};


NAMESPACE_SPH_END
