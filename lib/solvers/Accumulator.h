#pragma once

#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

/// Defines parity of the functor
enum class AccumulateParity {
    ONE_SIDED,    /// f(i, j) is generally different than f(j, i), must compute both values
    SYMMETRIC,    /// f(i, j) == f(j, i), will accumulate both values at one call
    ANTISYMMETRIC /// f(i, j) == -f(j, i), will accumulate both values at one call
};

/// Simple wrapper over array to simplify accumulating values for each interacting pair of particles.
/// This is useful for quantities where the value is determined by direct summation over neighbouring
/// particles rather than by solving evolutionary equation.

template <typename TValue, typename TAccumulate>
class Accumulator : public Noncopyable {
private:
    Array<TValue> values;
    TAccumulate functor;

public:
    void update(Storage& storage) {
        values.resize(storage.getParticleCnt());
        values.fill(TValue(0._f));
        functor.template update(storage);
    }

    INLINE void accumulate(const int i, const int j, const Vector& grad) {
        values[i] += functor(i, j, grad);
    }

    Array<TValue>& get() {
        return values;
    }

    INLINE TValue operator[](const int idx) {
        return values[idx];
    }
};


NAMESPACE_SPH_END
