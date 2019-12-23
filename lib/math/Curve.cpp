#include "math/Curve.h"
#include <algorithm>

NAMESPACE_SPH_BEGIN

class Vector4 {
private:
    Float v[4];

public:
    Vector4(const Float x1, const Float x2, const Float x3, const Float x4)
        : v{ x1, x2, x3, x4 } {}

    INLINE Float operator[](const Size i) const {
        ASSERT(unsigned(i) < 4);
        return v[i];
    }
};

static Float dot(const Vector4& v1, const Vector4& v2) {
    return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2] + v1[3] * v2[3];
}

class Matrix4 {
private:
    Vector4 rows[4];

public:
    Matrix4(const Vector4& v1, const Vector4& v2, const Vector4& v3, const Vector4& v4)
        : rows{ v1, v2, v3, v4 } {}

    Vector4 row(const Size i) const {
        return rows[i];
    }

    Vector4 operator*(const Vector4& v) const {
        return Vector4(dot(rows[0], v), dot(rows[1], v), dot(rows[2], v), dot(rows[3], v));
    }
};

const Matrix4 CURVE_MATRIX{
    { 1.f, 0.f, 0.f, 0.f },
    { 0.f, 0.f, 1.f, 0.f },
    { -3.f, 3.f, -2.f, -1.f },
    { 2.f, -2.f, 1.f, 1.f },
};

static Float getDist(const CurvePoint& p1, const CurvePoint& p2) {
    return sqrt(sqr(p1.x - p2.x) + sqr(p1.y - p2.y));
}

static Float getDerivative(const CurvePoint& p1, const CurvePoint& p2) {
    return (p2.y - p1.y) / (p2.x - p1.x);
}

static Float getDerivative(const CurvePoint& p0, const CurvePoint& p1, const CurvePoint& p2) {
    // centripetal parametrization -> weight by the squared root of the distance
    const Float d1 = sqrt(getDist(p0, p1));
    const Float d2 = sqrt(getDist(p1, p2));
    return lerp(getDerivative(p0, p1), getDerivative(p1, p2), d1 / (d1 + d2));
}


Curve::Curve() {
    points = {
        { { 0.f, 0.f }, true },
        { { 1.f, 1.f }, true },
    };
}

Curve::Curve(const Interval& rangeX, const Interval& rangeY) {
    points = {
        { { float(rangeX.lower()), float(rangeY.lower()) }, true },
        { { float(rangeX.upper()), float(rangeY.upper()) }, true },
    };
}

Curve::Curve(Array<CurvePoint>&& curve) {
    points.resize(curve.size());
    for (Size i = 0; i < curve.size(); ++i) {
        points[i] = std::make_pair(curve[i], true);
    }
    this->sort();
}

Curve::Curve(const Curve& curve) {
    *this = curve;
}

Curve& Curve::operator=(const Curve& curve) {
    points = curve.points.clone();
    return *this;
}

Size Curve::getPointCnt() const {
    return points.size();
}

const CurvePoint& Curve::getPoint(const Size idx) const {
    return points[idx].first;
}

void Curve::setPoint(const Size idx, const CurvePoint& newPoint) {
    points[idx].first = newPoint;
    this->sort();
}

void Curve::addPoint(const CurvePoint& newPoint) {
    auto iter = std::lower_bound(points.begin(),
        points.end(),
        newPoint,
        [](const std::pair<CurvePoint, bool>& p, const CurvePoint& q) { return p.first.x < q.x; });
    if (iter == points.end()) {
        points.emplaceBack(newPoint, points[points.size() - 2].second);
    } else {
        points.insert(iter - points.begin(), std::make_pair(newPoint, (iter - 1)->second));
    }
}

void Curve::deletePoint(const Size idx) {
    if (points.size() > 2) {
        points.remove(idx);
    }
}

bool Curve::getSegment(const Size idx) const {
    return points[idx].second;
}

void Curve::setSegment(const Size idx, const bool cubic) {
    points[idx].second = cubic;
}

Interval Curve::rangeX() const {
    Interval range;
    for (const auto& p : points) {
        range.extend(p.first.x);
    }
    return range;
}

Interval Curve::rangeY() const {
    Interval range;
    for (const auto& p : points) {
        range.extend(p.first.y);
    }
    return range;
}

Float Curve::operator()(const Float x) const {
    auto lowerBoundIter = std::lower_bound(
        points.begin(), points.end(), x, [](const std::pair<CurvePoint, bool>& p, const Float x) {
            return p.first.x < x;
        });
    Size idx;
    if (lowerBoundIter == points.end() || lowerBoundIter == points.begin()) {
        idx = 0;
    } else {
        idx = lowerBoundIter - points.begin() - 1;
    }

    auto leftDy = [this, idx] {
        return points[idx - 1].second
                   ? getDerivative(points[idx - 1].first, points[idx].first, points[idx + 1].first)
                   : getDerivative(points[idx - 1].first, points[idx].first);
    };
    auto rightDy = [this, idx] {
        return points[idx + 1].second
                   ? getDerivative(points[idx].first, points[idx + 1].first, points[idx + 2].first)
                   : getDerivative(points[idx + 1].first, points[idx + 2].first);
    };

    if (points[idx].second && points.size() > 2) {
        if (idx == 0) {
            return cubic(points[idx].first,
                points[idx + 1].first,
                getDerivative(points[idx].first, points[idx + 1].first),
                rightDy(),
                x);
        } else if (idx == points.size() - 2) {
            return cubic(points[idx].first,
                points[idx + 1].first,
                leftDy(),
                getDerivative(points[idx].first, points[idx + 1].first),
                x);
        } else {
            return cubic(points[idx].first, points[idx + 1].first, leftDy(), rightDy(), x);
        }
    } else {
        return linear(points[idx].first, points[idx + 1].first, x);
    }
}

Float Curve::linear(const CurvePoint& p1, const CurvePoint& p2, const Float x) const {
    return p1.y + (x - p1.x) / (p2.x - p1.x) * (p2.y - p1.y);
}

Float Curve::cubic(const CurvePoint& p1,
    const CurvePoint& p2,
    const Float dy1,
    const Float dy2,
    const Float x) const {
    const Float d = p2.x - p1.x;
    const Float t = (x - p1.x) / d;
    const Vector4 ts(1._f, t, pow<2>(t), pow<3>(t));
    const Vector4 ys(p1.y, p2.y, dy1 * d, dy2 * d);
    return dot(ts, CURVE_MATRIX * ys);
}

void Curve::sort() {
    std::sort(points.begin(),
        points.end(),
        [](const std::pair<CurvePoint, bool>& p1, const std::pair<CurvePoint, bool>& p2) {
            return p1.first.x < p2.first.x;
        });
}

NAMESPACE_SPH_END
