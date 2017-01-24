#pragma once

/// Objects for generating random vectors.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "math/rng/Rng.h"

NAMESPACE_SPH_BEGIN

/// Wrapper for generating random vectors. Takes RNG as template parameter, and can either keep
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

    /// Generates additional random numbers, can be viewed as extension of vector to more dimensions.
    /// \param i Index of dimension, must be at least 3 as first three dimensions belong to the vector.
    Float getAdditional(const Size i) {
        ASSERT(i >= 3);
        return rngImpl(i);
    }
};

/// Generic generator of random vectors using sampling with given PDF.
/// PDF does NOT have to be normalized (integral does not have to be 1).
/// \todo make jacobian work, create few basic coordinate systems
template <typename TScalarRng>
class VectorPdfRng : public Noncopyable {
private:
    Box box;
    VectorRng<TScalarRng> vectorRng;
    std::function<Float(const Vector&)> pdf;
    std::function<Float(const Vector&)> jacobian;
    Float maxPdf;

public:
    /// Construct a random vector generator.
    /// \param box bounds for minimal and maximal random values
    /// \param pdf Used PDF. Default value means uniform sampling. Does not have to be normalized, but must
    /// return strictly non-negative values.
    /// \param jacobian Used Jacobian if other coordinate system is to be used. Default is cartesian system.
    /// \param maximalPdf Maximal value of given PDF. By default, the value is estimated from the function
    /// itself.
    VectorPdfRng(const Box& box,
        const std::function<Float(const Vector&)>& pdf = [](const Vector&) { return 1._f; },
        const std::function<Float(const Vector&)>& jacobian = [](const Vector&) { return 1._f; },
        const Float maximalPdf = 0._f)
        : box(box)
        , pdf(pdf)
        , jacobian(jacobian)
        , maxPdf(maximalPdf) {
        if (maxPdf == 0._f) {
            // step for finding pdf maximum
            /// \todo should depend on jacobian
            const Vector delta = 0.05_f * box.size();
            /// \todo jacobian
            box.iterate(delta,
                [&](const Vector& v) { this->maxPdf = max(this->maxPdf, this->pdf(v) * this->jacobian(v)); });
        }
    }

    VectorPdfRng(VectorPdfRng&& other)
        : box(std::move(other.box))
        , vectorRng(std::move(other.vectorRng))
        , pdf(std::move(other.pdf))
        , jacobian(std::move(other.jacobian))
        , maxPdf(other.maxPdf) {}

    Vector operator()() {
        ASSERT(getLength(box.size()) > 0._f);
        for (Size i = 0; i < 1000; ++i) {
            Vector v = vectorRng() * box.size() + box.lower();
            const Float p = vectorRng.getAdditional(4) * maxPdf;
            if (p < pdf(v) * jacobian(v)) {
                return v;
            }
        }
        ASSERT(false && "Couldn't generate vector in 1000 iterations");
        return Vector(0._f);
    }
};


NAMESPACE_SPH_END
