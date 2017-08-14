#pragma once

/// \file Approx.h
/// \brief Helper wrapper that allows to check whether two values are equal to some given accuracy.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/geometry/TracelessTensor.h"

NAMESPACE_SPH_BEGIN

namespace Detail {
    /// Type trait checking whether type T has overloaded operator <<
    template <typename T, typename TStream, typename TEnabler = void>
    struct IsPrintable {
        static constexpr bool value = false;
    };

    template <typename T, typename TStream>
    struct IsPrintable<T, TStream, std::void_t<decltype(std::declval<TStream&>() << std::declval<T>())>> {
        static constexpr bool value = true;
    };

    static_assert(IsPrintable<float, std::ostream>::value, "float must be printable");
    static_assert(!IsPrintable<void, std::ostream>::value, "void must not be printable");
}

/// This is more or less stolen from Catch unit-testing framework.
template <typename Type>
class Approx {
private:
    Float epsilon;
    Type value;

public:
    INLINE explicit Approx(const Type value)
        : epsilon(EPS)
        , value(value) {}

    INLINE Approx operator()(const Type& value) {
        Approx approx(value);
        approx.setEpsilon(epsilon);
        return approx;
    }

    INLINE friend bool operator==(const Type& lhs, const Approx& rhs) {
        return almostEqual(lhs, rhs.value, rhs.epsilon);
    }

    INLINE friend bool operator==(const Approx& lhs, const Type& rhs) {
        return operator==(rhs, lhs);
    }

    INLINE friend bool operator!=(const Type& lhs, const Approx& rhs) {
        return !operator==(lhs, rhs);
    }

    INLINE friend bool operator!=(const Approx& lhs, const Type& rhs) {
        return !operator==(rhs, lhs);
    }

    INLINE Approx& setEpsilon(const Float newEps) {
        epsilon = newEps;
        return *this;
    }

    friend std::ostream& operator<<(std::ostream& stream, const Approx& approx) {
        stream << "~" << approx.value << " (eps = " << approx.epsilon << ")";
        return stream;
    }
};

/// We have to wait till c++17 for type deduction in constructors ...
template <typename Type>
Approx<Type> approx(const Type& value, const Float eps = EPS) {
    return Approx<Type>(value).setEpsilon(eps);
}


NAMESPACE_SPH_END
