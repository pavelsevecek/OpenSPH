#pragma once

/// \file OperatorTemplate.h
/// \brief Class defining additional operators from existing ones
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/Assert.h"

NAMESPACE_SPH_BEGIN

/// \brief Class defining additional operators from existing ones
///
/// Useful to define an arithmetic types, without explicitly listing all the necessary operators.
/// The derived class can use all or just few of defined operators. The operators come in packs; if the
/// derived class defines operators +=, OperatorTemplate also provides binary operator +. If it further
/// defines an unary operator -, the OperatorTemplate provides operators -= and binary -.
///
/// Besides these, the class can implement operator *=(float), i.e. multiplication by scalar. The
/// OperatorTemplate then provides binary operator *(class, float) and *(float, class), and also operator /=
/// and operator/(class, float). This 'pack' of operators is independent from the summation/subtraction
/// operators; both can be implemented, or just one of them.
///
/// Finally, the derived class can implement the equality operator==, the OperatorTemplate then also provides
/// operator !=. This pair of operators is also independent from all previous operators.
template <typename TDerived>
struct OperatorTemplate {
public:
    TDerived operator+(const TDerived& other) const {
        TDerived sum(derived());
        sum += other;
        return sum;
    }
    friend TDerived operator-(const TDerived& value1, const TDerived& value2) {
        // this has to be a friend operator as the unary operator would hide the binary one (we would have to
        // add using OperatorTemplate<...>::operator-, which is undesirable)
        return value1 + (-value2);
    }
    TDerived& operator-=(const TDerived& other) {
        return derived().operator+=(-other);
    }
    bool operator!=(const TDerived& other) const {
        return !(derived() == other);
    }
    /// \todo gcc is happy even if the operators arent defined (as long as they are not called), is this
    /// correct according to standard?
    TDerived operator*(const Float value) const {
        TDerived multiplied(derived());
        multiplied *= value;
        return multiplied;
    }
    friend TDerived operator*(const Float value, const TDerived& derived) {
        return derived * value;
    }
    TDerived operator/(const Float value) const {
        ASSERT(value != 0._f);
        return derived() * (1._f / value);
    }
    TDerived& operator/=(const Float value) {
        ASSERT(value != 0._f);
        return derived().operator*=(1._f / value);
    }

private:
    INLINE TDerived& derived() {
        return static_cast<TDerived&>(*this);
    }

    INLINE const TDerived& derived() const {
        return static_cast<const TDerived&>(*this);
    }
};

NAMESPACE_SPH_END
