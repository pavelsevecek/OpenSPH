#pragma once

/// \file AtomicFloat.h
/// \brief Implementation of number with atomic operators.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/Assert.h"
#include "common/Globals.h"
#include <atomic>

NAMESPACE_SPH_BEGIN

/// \brief Atomic value implemented using compare-exchange.
///
/// This is mainly intended for floating-point values (floats and doubles), as atomic operations for integral
/// types are supplied by the compiler.
template <typename Type>
class Atomic {
private:
    std::atomic<Type> value;

public:
    INLINE Atomic() = default;

    INLINE Atomic(const Type f) {
        value.store(f);
    }

    INLINE Atomic(const Atomic& other)
        : Atomic(other.value.load()) {}

    INLINE Atomic& operator=(const Type f) {
        value.store(f);
        return *this;
    }

    INLINE Atomic& operator+=(const Type f) {
        atomicOp(f, [](const Type lhs, const Type rhs) { return lhs + rhs; });
        return *this;
    }

    INLINE Atomic& operator-=(const Type f) {
        atomicOp(f, [](const Type lhs, const Type rhs) { return lhs - rhs; });
        return *this;
    }

    INLINE Atomic& operator*=(const Type f) {
        atomicOp(f, [](const Type lhs, const Type rhs) { return lhs * rhs; });
        return *this;
    }

    INLINE Atomic& operator/=(const Type f) {
        ASSERT(f != 0._f);
        atomicOp(f, [](const Type lhs, const Type rhs) { return lhs / rhs; });
        return *this;
    }

    INLINE Type operator+(const Type f) const {
        return value.load() + f;
    }

    INLINE Type operator-(const Type f) const {
        return value.load() - f;
    }

    INLINE Type operator*(const Type f) const {
        return value.load() * f;
    }

    INLINE Type operator/(const Type f) const {
        ASSERT(f != 0._f);
        return value.load() / f;
    }

    INLINE bool operator==(const Type f) const {
        return value.load() == f;
    }

    INLINE bool operator!=(const Type f) const {
        return value.load() != f;
    }

    INLINE bool operator>(const Type f) const {
        return value.load() > f;
    }

    INLINE bool operator<(const Type f) const {
        return value.load() < f;
    }

    INLINE friend std::ostream& operator<<(std::ostream& stream, const Atomic& f) {
        stream << f.value.load();
        return stream;
    }

private:
    template <typename TOp>
    INLINE Type atomicOp(const Type rhs, const TOp& op) {
        Type lhs = value.load();
        Type desired = op(lhs, rhs);
        while (!value.compare_exchange_weak(lhs, desired)) {
            desired = op(lhs, rhs);
        }
        return desired;
    }
};

NAMESPACE_SPH_END
