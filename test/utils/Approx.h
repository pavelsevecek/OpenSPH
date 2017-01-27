#pragma once

/// Helper wrapper that allows to check whether two values are equal to some given accuracy.
/// This is more or less stolen from Catch unit-testing framework, with overloaded consturctors for vectors
/// and tensors.

#include "geometry/TracelessTensor.h"

NAMESPACE_SPH_BEGIN

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

    INLINE friend bool operator==(const Approx& lhs, const Type& rhs) { return operator==(rhs, lhs); }

    INLINE friend bool operator!=(const Type& lhs, const Approx& rhs) { return !operator==(lhs, rhs); }

    INLINE friend bool operator!=(const Approx& lhs, const Type& rhs) { return !operator==(rhs, lhs); }

    INLINE Approx& setEpsilon(const Float newEps) {
        epsilon = newEps;
        return *this;
    }

    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const Approx& approx) {
        stream << "~" << approx.value;
        return stream;
    }
};

/// We have to wait till c++17 for type deduction in constructors ...
template<typename Type>
Approx<Type> approx(const Type& value, const Float eps = EPS) {
    return Approx<Type>(value).setEpsilon(eps);
}


NAMESPACE_SPH_END
