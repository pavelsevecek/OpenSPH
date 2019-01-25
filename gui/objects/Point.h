#pragma once

/// \file Point.h
/// \brief Simple 2D vector with integer coordinates. Provides conversion from and to wxPoint.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "math/MathUtils.h"
#include "objects/Object.h"
#include <wx/gdicmn.h>

NAMESPACE_SPH_BEGIN

template <typename T, typename TDerived>
struct BasicPoint {
    T x, y;

    BasicPoint() = default;

    BasicPoint(const T& x, const T& y)
        : x(x)
        , y(y) {}

    T& operator[](const Size index) {
        ASSERT(index < 2);
        return reinterpret_cast<T*>(this)[index];
    }

    const T& operator[](const Size index) const {
        ASSERT(index < 2);
        return reinterpret_cast<const T*>(this)[index];
    }

    TDerived& operator+=(const TDerived& other) {
        x += other.x;
        y += other.y;
        return static_cast<TDerived&>(*this);
    }

    TDerived& operator-=(const TDerived& other) {
        x -= other.x;
        y -= other.y;
        return static_cast<TDerived&>(*this);
    }

    TDerived& operator*=(const float factor) {
        x *= factor;
        y *= factor;
        return static_cast<TDerived&>(*this);
    }

    TDerived operator+(const TDerived& other) const {
        TDerived result(static_cast<const TDerived&>(*this));
        result += other;
        return result;
    }

    TDerived operator-(const TDerived& other) const {
        TDerived result(static_cast<const TDerived&>(*this));
        result -= other;
        return result;
    }

    TDerived operator*(const float factor) const {
        TDerived result(static_cast<const TDerived&>(*this));
        result *= factor;
        return result;
    }

    bool operator==(const TDerived& other) const {
        return x == other.x && y == other.y;
    }
};

struct Pixel : public BasicPoint<int, Pixel> {
    using BasicPoint<int, Pixel>::BasicPoint;

    explicit Pixel(const wxPoint point)
        : BasicPoint<int, Pixel>(point.x, point.y) {}

    explicit operator wxPoint() const {
        return wxPoint(this->x, this->y);
    }
};

struct Coords : public BasicPoint<float, Coords> {
    using BasicPoint<float, Coords>::BasicPoint;

    explicit Coords(const Pixel p)
        : BasicPoint<float, Coords>(p.x, p.y) {}

    Coords operator*(const Coords& other) const {
        Coords res = *this;
        res.x *= other.x;
        res.y *= other.y;
        return res;
    }

    Coords operator/(const Coords& other) const {
        Coords res = *this;
        ASSERT(other.x != 0.f && other.y != 0.f);
        res.x /= other.x;
        res.y /= other.y;
        return res;
    }

    explicit operator Pixel() const {
        return Pixel(int(x), int(y));
    }

    explicit operator wxPoint() const {
        return wxPoint(int(x), int(y));
    }
};

template <typename T, typename TDerived>
INLINE float getLength(const BasicPoint<T, TDerived>& p) {
    return sqrt(sqr(p.x) + sqr(p.y));
}

NAMESPACE_SPH_END
