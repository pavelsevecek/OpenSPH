#pragma once

/// Objects for generating random vectors.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "math/rng/Rng.h"

NAMESPACE_SPH_BEGIN

/// Wrapper for generating random d-dimensional vectors. Takes RNG as template parameter, and can either keep
/// reference to other RNG object, or create RNG object of its own.
template <typename TScalarRng>
class VectorRng : public Noncopyable {
private:
    TScalarRng rngImpl;

public:
    /// Default constructor. Enabled only if RNG is owned by the object.
    template <typename = std::enable_if_t<!std::is_reference<TScalarRng>::value>>
    VectorRng() {}

    /// \todo this should copy l-value ref and move r-value ref, right?
    VectorRng(TScalarRng&& rng)
        : rngImpl(std::forward<TScalarRng>(rng)) {}

    template <typename = std::enable_if_t<!std::is_reference<TScalarRng>::value>>
    VectorRng(VectorRng&& other)
        : rngImpl(std::move(other.rngImpl)) {}

    template <typename = std::enable_if_t<!std::is_reference<TScalarRng>::value>>
    VectorRng& operator=(VectorRng&& other) {
        rngImpl = std::move(other.rngImpl);
        return *this;
    }

    Vector operator()() { return Vector(rngImpl(0), rngImpl(1), rngImpl(2)); }

    Float getAdditional(const int i) {
        ASSERT(i >= 3); // first 3 indices taken by vector
        return rngImpl(i);
    }
};

/// Generic generator of random vectors using sampling with given PDF.
/// PDF must have units such that \int pdf(x) jacobian(x) dV = dimensionless, however it does NOT have to be
/// normalized (integral does not have to be 1)
/// \todo make jacobian work, create few basic coordinate systems
template <typename TScalarRng, typename TPdf, typename TJacobian>
class VectorPdfRng : public Noncopyable {
    static_assert(!std::is_reference<TPdf>::value, "Cannot use with reference Pdf");
    static_assert(!std::is_reference<TJacobian>::value, "Cannot use with reference Jacobian");

private:
    Box box;
    TPdf pdf;
    TJacobian jac;
    VectorRng<TScalarRng> vectorRng;
    Float maxPdf;

public:
    /// Construct a random vector generator.
    /// Cannot be used directly unless template parameters are explicitly specified.
    /// Use makeVectorRng if the type is unknown (lambdas)
    /// \param box bounds for minimal and maximal random values
    /// \param rng underlying uniform random-number generator.
    ///            Must implement T operator()(int component).
    /// \param lambda used pdf. Must return PdfType, does NOT have to be normalized. Default is uniform
    /// sampling.
    /// \param jacobian used jacobian if other coordinate system is used. Default is cartesian system.
    VectorPdfRng(const Box& box,
                 TScalarRng&& rng,
                 TPdf&& lambda        = [](const Vector&) { return 1._f; },
                 TJacobian&& jacobian = [](const Vector&) { return 1._f; })
        : box(box)
        , pdf(std::move(lambda))
        , jac(std::move(jacobian))
        , vectorRng(std::forward<TScalarRng>(rng)) {
        // step for finding pdf maximum
        // TODO: should depend on jacobian
        const Vector delta = 0.05_f * box.getSize();
        maxPdf             = 0._f;
        /// \todo jacobian
        box.iterate(delta, [this](const Vector& v) { maxPdf = Math::max(maxPdf, pdf(v) * jac(v)); });
    }

    VectorPdfRng(VectorPdfRng&& other)
        : box(std::move(other.box))
        , pdf(std::move(other.pdf))
        , jac(std::move(other.jac))
        , vectorRng(std::move(other.vectorRng))
        , maxPdf(other.maxPdf) {}


    Vector operator()() {
        ASSERT(getLength(box.getSize()) > 0._f);
        while (true) {
            Vector v     = vectorRng() * box.getSize() + box.lower();
            const auto p = vectorRng.getAdditional(4) * maxPdf;
            if (p < pdf(v) * jac(v)) {
                return v;
            }
        }
    }
};


template <typename TRng, typename TPdf, typename TJacobian>
auto makeVectorPdfRng(const Box& box,
                      TRng&& rng,
                      TPdf&& lambda        = [](const Vector&) { return 1._f; },
                      TJacobian&& jacobian = [](const Vector&) { return 1._f; }) {
    return VectorPdfRng<TRng, TPdf, TJacobian>(box,
                                               std::forward<TRng>(rng),
                                               std::forward<TPdf>(lambda),
                                               std::forward<TJacobian>(jacobian));
}


template <typename TRng, typename TPdf>
auto makeVectorPdfRng(const Box& box, TRng&& rng, TPdf&& lambda = [](const Vector&) { return 1._f; }) {
    return makeVectorPdfRng(box,
                            std::forward<TRng>(rng),
                            std::forward<TPdf>(lambda),
                            [](const Vector&) { return 1._f; });
}

template <typename TRng>
auto makeVectorPdfRng(const Box& box, TRng&& rng) {
    return makeVectorPdfRng(box, std::forward<TRng>(rng), [](const Vector&) {
        return 1._f;
    });
}
/*template <typename T, int d, typename TRng, typename TPdf>
auto makeBoxRng(const Box<T, d> bounds, const TRng& rng, const TPdf& pdf) {
    VectorRng<T, d, TRng, TPdf> vectorRng(rng, pdf);
    vectorRng.setBoxPdf(bounds);
    return vectorRng;
}*/
/*
template <int d, class TRng, typename TPdf, typename T, typename... TArgs>
class SphericalPdfRng : public TRng {
private:
    TPdf pdf;
    T maxPdf;
    const Vector<T, d> center;
    const T radius;

protected:
    // uniform sampling on circle
    Vector<T, 2> randSphere(const Vector<T, 2>& center = Vector<T, 2>(0.f), const T radius = 1.f) {
        TRng& rng   = *static_cast<TRng*>(this);
        const T r   = Math::root<2>(rng.getRand(0)) * radius;
        const T phi = 2 * Math::PI<T> * rng.getRand(1);
        return center + r * Vector<T, 2>(Math::cos(phi), Math::sin(phi));
    }
    // uniform sampling on sphere
    Vector<T, 3> randSphere(const Vector<T, 3>& center = Vector<T, 3>(0.f), const T radius = 1.f) {
        TRng& rng        = *static_cast<TRng*>(this);
        const T r        = Math::root<3>(rng.getRand(0)) * radius;
        const T phi      = 2 * Math::PI<T> * rng.getRand(1);
        const T cosTheta = 1.f - 2.f * rng.getRand(2);
        const T sinTheta = Math::sqrt(1.f - Math::sqr(cosTheta));
        return center + r * Vector<T, 3>(sinTheta * Math::cos(phi), sinTheta * Math::sin(phi), cosTheta);
    }

public:
    SphericalPdfRng(const Vector<T, 3>& center, const T radius, TPdf lambda, const float step = 0.01f)
        : pdf(lambda)
        , center(center)
        , radius(radius) {
        // find maximal PDF value
        const T deltaRadial  = step * radius;
        const T deltaAngular = step * 2.f * Math::PI<T>;
        maxPdf               = 0.f;
        for (T r = 0.f; r < radius; r += deltaRadial) {
            for (T phi = 0.f; phi < 2.f * Math::PI<T>; phi += deltaAngular) {
                for (float theta = 0.f; theta < Math::PI<T>; theta += deltaAngular) {
                    maxPdf = Math::max(maxPdf, pdf(Vector<T, 3>::spherical(r, theta, phi)));
                }
            }
        }
    }
    SphericalPdfRng(const Vector<T, 2>& center, const T radius, TPdf lambda, const float step = 0.01f)
        : pdf(lambda)
        , center(center)
        , radius(radius) {
        // find maximal PDF value
        const T deltaRadial  = step * radius;
        const T deltaAngular = step * 2.f * Math::PI<T>;
        maxPdf               = 0.f;
        for (T r = 0.f; r < radius; r += deltaRadial) {
            for (T phi = 0.f; phi < 2.f * Math::PI<T>; phi += deltaAngular) {
                maxPdf = Math::max(maxPdf, pdf(Vector<T, 2>::spherical(r, phi)));
            }
        }
    }

    Vector<T, d> rand() {
        TRng& rng = *static_cast<TRng*>(this);
        while (true) {
            Vector<T, d> v = randSphere(center, radius);
            float p = rng.getRand(d) * maxPdf;
            if (p < pdf(v)) {
                return v;
            }
        }
    }
};

template <class TVector, class TRng, typename TPdf>
decltype(auto) makeSphericalRng(const TVector vector,
                                const typename TVector::Precision radius,
                                TRng&,
                                const TPdf& pdf) {
    return SphericalPdfRng<TVector::dimensions(), typename TVector::Precision, TRng, TPdf>(vector,
                                                                                           radius,
                                                                                           pdf);
}
*/

NAMESPACE_SPH_END
