#pragma once

/// \file Point.h
/// \brief 2D point and other primitives for 2D geometry
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/utility/OperatorTemplate.h"

NAMESPACE_SPH_BEGIN

/// \brief Point in 2D plot
///
/// \todo deduplicate with Gui Point class
struct PlotPoint : public OperatorTemplate<PlotPoint> {
    Float x, y;

    PlotPoint() = default;

    PlotPoint(const Float x, const Float y)
        : x(x)
        , y(y) {}

    PlotPoint& operator+=(const PlotPoint& other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    bool operator==(const PlotPoint& other) const {
        return x == other.x && y == other.y;
    }
};

/// \brief Point with error bars
struct ErrorPlotPoint : public PlotPoint {
    Float dx, dy;
};

/// \brief 2D affine matrix
///
/// Represets a generic linear transform + translation of a point
class AffineMatrix2 : public OperatorTemplate<AffineMatrix2> {
private:
    Float data[6];

public:
    /// \brief Creates the matrix given a uniform scaling factor and a translation vector
    ///
    /// By default, an identity matrix is constructed.
    /// \param scale Scaling factor
    /// \param translation Translation vector
    AffineMatrix2(const Float scale = 1._f, const PlotPoint translation = PlotPoint(0._f, 0._f))
        : data{ scale, 0._f, translation.x, 0._f, scale, translation.y } {}

    /// \brief Creates the matrix from individual components.
    AffineMatrix2(const Float xx,
        const Float yx,
        const Float xy,
        const Float yy,
        const Float tx = 0._f,
        const Float ty = 0._f)
        : data{ xx, yx, tx, xy, yy, ty } {}

    /// \brief Returns the given component of the matrix
    Float& operator()(const Size i, const Size j) {
        ASSERT(i < 2 && j < 3);
        return data[3 * i + j];
    }

    /// \brief Returns the given component of the matrix
    Float operator()(const Size i, const Size j) const {
        ASSERT(i < 2 && j < 3);
        return data[3 * i + j];
    }

    /// \brief Returns the given component of the matrix.
    ///
    /// Syntactic sugar.
    Float get(const Size i, const Size j) const {
        return operator()(i, j);
    }

    /// \brief Applies the affine transform on given point
    PlotPoint transformPoint(const PlotPoint& p) const {
        return PlotPoint(
            get(0, 0) * p.x + get(0, 1) * p.y + get(0, 2), get(1, 0) * p.x + get(1, 1) * p.y + get(1, 2));
    }

    /// \brief Applies the transform on given vector.
    ///
    /// This does not apply the translation.
    PlotPoint transformVector(const PlotPoint& p) const {
        return PlotPoint(get(0, 0) * p.x + get(0, 1) * p.y, get(1, 0) * p.x + get(1, 1) * p.y);
    }

    /// \brief Returns the inverse of the matrix
    ///
    /// The matrix must be invertible, checked by assert.
    AffineMatrix2 inverse() const {
        const Float a = get(0, 0);
        const Float b = get(0, 1);
        const Float c = get(1, 0);
        const Float d = get(1, 1);
        const Float tx = get(0, 2);
        const Float ty = get(1, 2);

        const Float det = a * d - b * c;
        const Float detInv = 1._f / det;
        ASSERT(det != 0._f);

        return AffineMatrix2(d * detInv,
            -b * detInv,
            -c * detInv,
            a * detInv,
            -(d * tx - b * ty) * detInv,
            (c * tx - a * ty) * detInv);
    }

    bool operator==(const AffineMatrix2& other) const {
        for (Size i = 0; i < 6; ++i) {
            if (data[i] != other.data[i]) {
                return false;
            }
        }
        return true;
    }
};


NAMESPACE_SPH_END
