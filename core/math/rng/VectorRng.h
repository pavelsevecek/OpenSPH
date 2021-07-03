#pragma once

/// \file VectorRng.h
/// \brief Objects for generating random vectors.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "math/rng/Rng.h" // not really needed, but it doesn't make sense to use VectorRng without some Rng object
#include "objects/wrappers/Function.h"

NAMESPACE_SPH_BEGIN

/// \brief Wrapper for generating random vectors.
///
/// Takes RNG as template parameter, and can either keep reference to other RNG object, or create RNG object
/// of its own.
template <typename TScalarRng>
class VectorRng : public Noncopyable {
private:
    TScalarRng rngImpl;

public:
    /// Default constructor. Can be only used if RNG is owned by the object.
    VectorRng() {
        static_assert(!std::is_reference<TScalarRng>::value, "Cannot be used for references");
    }

    template <typename... TArgs>
    explicit VectorRng(TArgs&&... args)
        : rngImpl(std::forward<TArgs>(args)...) {}

    VectorRng(VectorRng&& other)
        : rngImpl(std::move(other.rngImpl)) {
        static_assert(!std::is_reference<TScalarRng>::value, "Cannot be used for references");
    }

    VectorRng& operator=(VectorRng&& other) {
        static_assert(!std::is_reference<TScalarRng>::value, "Cannot be used for references");
        rngImpl = std::move(other.rngImpl);
        return *this;
    }

    Vector operator()() {
        return Vector(rngImpl(0), rngImpl(1), rngImpl(2));
    }

    /// Generates additional random numbers, can be viewed as extension of vector to more dimensions.
    /// \param i Index of dimension, must be at least 3 as first three dimensions belong to the vector.
    Float getAdditional(const Size i) {
        SPH_ASSERT(i >= 3);
        return rngImpl(i);
    }
};

/// \brief Generic generator of random vectors using sampling with given PDF.
///
/// PDF does NOT have to be normalized (integral does not have to be 1).
/// \todo make jacobian work, create few basic coordinate systems
template <typename TScalarRng>
class VectorPdfRng : public Noncopyable {
private:
    Box box;
    VectorRng<TScalarRng> vectorRng;
    Function<Float(const Vector&)> pdf;
    Function<Float(const Vector&)> jacobian;
    Float maxPdf;

public:
    /// Construct a random vector generator.
    /// \param box bounds for minimal and maximal random values
    /// \param pdf Used PDF. Default value means uniform sampling. Does not have to be normalized, but must
    /// return strictly non-negative values.
    /// \param jacobian Used Jacobian if other coordinate system is to be used. Default is cartesian system.
    /// \param maximalPdf Maximal value of given PDF. By default, the value is estimated from the function
    /// itself.
    VectorPdfRng(
        const Box& box,
        const Function<Float(const Vector&)>& pdf = [](const Vector&) { return 1._f; },
        const Function<Float(const Vector&)>& jacobian = [](const Vector&) { return 1._f; },
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
            box.iterate(delta, [&](const Vector& v) { //
                this->maxPdf = max(this->maxPdf, this->pdf(v) * this->jacobian(v));
            });
            SPH_ASSERT(maxPdf > 0._f);
        }
    }

    Vector operator()() {
        SPH_ASSERT(getLength(box.size()) > 0._f);
        for (Size i = 0; i < 10000; ++i) {
            Vector v = vectorRng() * box.size() + box.lower();
            const Float p = vectorRng.getAdditional(4) * maxPdf;
            if (p < pdf(v) * jacobian(v)) {
                return v;
            }
        }
        SPH_ASSERT(false, "Couldn't generate vector in 1000 iterations");
        return Vector(0._f);
    }
};


NAMESPACE_SPH_END
