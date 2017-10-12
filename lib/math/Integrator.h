#pragma once

/// \file Integrator.h
/// \author Basic routines for integrating arbitrary functions in N dimensions
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "math/rng/Rng.h"
#include "objects/geometry/Domain.h"
#include "objects/wrappers/AutoPtr.h"

NAMESPACE_SPH_BEGIN

/// Integrate a one-dimensional function using Simpson's rule
template <typename TFunctor>
INLINE Float integrate(const Interval range, TFunctor functor) {
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


/// Object for integrating a generic three-dimensional scalar function.
template <typename TRng = UniformRng, typename TInternal = double>
class Integrator : public Noncopyable {
private:
    Float radius;
    TRng rng;
    AutoPtr<IDomain> domain;

    static constexpr uint chunk = 100;

public:
    /// \brief Constructs an integrator given domain of integration.
    Integrator(AutoPtr<IDomain>&& domain)
        : domain(std::move(domain)) {}

    /// \brief Integrates a function.
    /// \param f Functor with a Vector parameter, returning the integral as a scalar value
    /// \param targetError Precision of the integral. Note that half error means approximately four times the
    ///                    computation time.
    template <typename TFunctor>
    Float integrate(TFunctor&& f, const Float targetError = 0.001_f) {
        double sum = 0.;
        double sumSqr = 0.;
        double errorSqr = sqr(targetError);
        Size n = 0;
        StaticArray<Vector, chunk> buffer;
        Array<Size> inside;
        Box box = domain->getBoundingBox();
        while (true) {
            for (Size i = 0; i < chunk; ++i) {
                // dimensionless vector
                const Vector q(rng(0), rng(1), rng(2));
                // vector properly scaled and moved
                const Vector v = box.lower() + q * box.size();
                buffer[i] = v;
            }
            inside.clear();
            domain->getSubset(buffer, inside, SubsetType::INSIDE);
            for (Size i : inside) {
                const double x = (double)f(buffer[i]);
                sum += x;
                sumSqr += x * x;
                n++;
            }
            const double m = double(n);
            if (m * sumSqr - sum * sum < m * m * errorSqr * sumSqr) {
                return Float(sum / m) * domain->getVolume();
            }
        }
    }
};

NAMESPACE_SPH_END
