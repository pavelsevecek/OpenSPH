#pragma once

/// \file Functional.h
/// \author Basic routines for integrating arbitrary functions in N dimensions, finding roots, etc.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "math/rng/Rng.h"
#include "objects/geometry/Domain.h"
#include "objects/wrappers/AutoPtr.h"

NAMESPACE_SPH_BEGIN

/// \brief Integrates a one-dimensional function using Simpson's rule.
///
/// The precision of the integral is currently fixed.
template <typename TFunctor>
INLINE Float integrate(const Interval range, const TFunctor& functor) {
    const Size N = 1000;
    const Float h = range.size() / N;
    double result = functor((Float)range.lower()) + functor((Float)range.upper());
    for (Size j = 1; j < N; ++j) {
        const Float x = (Float)range.lower() + j * h;
        if (j % 2 == 0) {
            result += 2._f * functor(x);
        } else {
            result += 4._f * functor(x);
        }
    }
    return Float(result * h / 3._f);
}


/// \brief Object for integrating a generic three-dimensional scalar function.
template <typename TRng = UniformRng, typename TInternal = double>
class Integrator : public Noncopyable {
private:
    const IDomain& domain;
    TRng rng;

    static constexpr uint chunk = 100;

public:
    /// \brief Constructs an integrator given the domain of integration.
    Integrator(const IDomain& domain)
        : domain(domain) {}

    Integrator(IDomain&& domain) = delete;

    /// \brief Integrates a function.
    ///
    /// \param f Functor with a Vector parameter, returning the integral as a scalar value
    template <typename TFunctor>
    Float integrate(TFunctor&& f, const Float targetError = 0.001_f) {
        double sum = 0.;
        double sumSqr = 0.;
        double errorSqr = sqr(targetError);
        Size n = 0;
        StaticArray<Vector, chunk> buffer;
        Array<Size> inside;
        Box box = domain.getBoundingBox();
        while (true) {
            for (Size i = 0; i < chunk; ++i) {
                // dimensionless vector
                const Vector q(rng(0), rng(1), rng(2));
                // vector properly scaled and moved
                const Vector v = box.lower() + q * box.size();
                buffer[i] = v;
            }
            inside.clear();
            domain.getSubset(buffer, inside, SubsetType::INSIDE);
            for (Size i : inside) {
                const double x = (double)f(buffer[i]);
                sum += x;
                sumSqr += x * x;
                n++;
            }
            const double m = double(n);
            if (m * sumSqr - sum * sum < m * m * errorSqr * sumSqr) {
                return Float(sum / m) * domain.getVolume();
            }
        }
    }
};

/// \brief Returns a root of a R->R function on given range.
///
/// If there is no root or the root cannot be found, returns NOTHING. For functions with multiple roots,
/// returns one of them; the selection of such root is not specified.
template <typename TFunctor>
INLINE Optional<Float> getRoot(const Interval& range, const Float eps, const TFunctor& functor) {
    Interval r = range;
    if (functor(r.lower()) * functor(r.upper()) > 0._f) { // same sign
        return NOTHING;
    }
    while (r.size() > eps * range.size()) {
        Float x = r.center();
        if (functor(x) * functor(r.upper()) > 0._f) {
            r = Interval(r.lower(), x);
        } else {
            r = Interval(x, r.upper());
        }
    }
    return r.center();
}

/// \brief Checks if the given R->R function is continuous in given interval.
///
/// The eps and delta values are the same for all values and must be specified by the caller.
template <typename TFunctor>
INLINE bool isContinuous(const Interval& range, const Float delta, const Float eps, const TFunctor& functor) {
    Float y1 = functor(range.lower());
    for (Float x = range.lower(); x <= range.upper(); x += delta) {
        const Float y2 = functor(x);
        if (abs(y1 - y2) > eps) {
            return false;
        }
        y1 = y2;
    }
    return true;
}

NAMESPACE_SPH_END
