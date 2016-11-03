#pragma once

/// Basic routines for integrating arbitrary functions in N dimensions
/// Pavel Sevecek 2015
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Domain.h"
#include "math/rng/Rng.h"

NAMESPACE_SPH_BEGIN

/// Object for integrating a generic scalar function.
template <typename TRng = UniformRng, typename TInternal = double>
class Integrator : public Noncopyable {
private:
    Float radius;
    TRng rng;
    Abstract::Domain* domain;
    static constexpr int chunk = 100;

public:
    /// Constructs an integrator given domain of integration.
    Integrator(Abstract::Domain* domain)
        : domain(domain) {}

    /// Integrate a function.
    /// \param f Functor with a parameter Vector<T, d>, returning a scalar value (of type T)
    /// \param targetError Precision of the integral. Note that half error means approximately four times the
    ///                    computation time.
    template <typename TFunctor>
    auto integrate(TFunctor&& f, const Float targetError = 0.001_f) {
        double sum = 0.;
        double sumSqr = 0.;
        double errorSqr = Math::sqr(targetError);
        int64_t n              = 0;
        StaticArray<Vector, chunk> buffer;
        StaticArray<bool, chunk> inside;
        Vector center = domain->getCenter();
        Float radius = domain->getBoundingRadius();
        while (true) {
            for (int i = 0; i < chunk; ++i) {
                // dimensionless vector
                /// \todo number of components must be d !!
                const Vector q(rng(0), rng(1), rng(2));
                // vector with units and properly scaled and moved
                const Vector v = radius * (2._f * q - Vector(1._f)) + center;
                buffer[i] = v;
            }
            domain->areInside(buffer, inside);
            for (int i = 0; i < chunk; ++i) {
                if (inside[i]) {
                    const double x = (double)f(buffer[i]);
                    sum += x;
                    sumSqr += x * x;
                    n++;
                }
            }
            //  const TResult e = (n*sumSqr - sum*sum)/(n*sumSqr);
            // std::cout << e << std::endl;
            // standard deviation = error = sqrt(variance) !!!
            // std::cout << "ERROR = " << 1.f/n* (n*sumSqr - sum*sum) / (n*sumSqr) << std::endl;
            const double m = double(n);
            if (m * sumSqr - sum * sum < m * m * errorSqr * sumSqr) {
                return Float(sum / m) * domain->getVolume();
            }
        }
    }
};

NAMESPACE_SPH_END
