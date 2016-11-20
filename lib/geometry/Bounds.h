#pragma once

/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Vector.h"
#include "geometry/Indices.h"
#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

/// Helper object defining three-dimensional interval (box).
class Box : public Object {
private:
    Vector minBound;
    Vector maxBound;

public:
    Box() {
        minBound = Vector(INFTY);
        maxBound = Vector(-INFTY);
    }

    Box(const Vector& minBound, const Vector& maxBound)
        : minBound(minBound)
        , maxBound(maxBound) {}

    /// Enlarges the box to contain the vector;
    void extend(const Vector& v) {
        maxBound = Math::max(maxBound, v);
        minBound = Math::min(minBound, v);
    }

    /// Checks if the vector lies inside the box
    bool contains(const Vector& v) const {
        for (int i = 0; i < 3; ++i) {
            if (v[i] < minBound[i] || v[i] > maxBound[i]) {
                return false;
            }
        }
        return true;
    }

    /// Clamps all components of the vector to fit within the box
    Vector clamp(const Vector& v) const {
        return Math::clamp(v, minBound, maxBound);
    }

    INLINE const Vector& lower() const { return minBound; }

    INLINE Vector& lower() { return minBound; }

    INLINE const Vector& upper() const { return maxBound; }

    INLINE Vector& upper() { return maxBound; }

    INLINE Vector getSize() const { return maxBound - minBound; }

    Float getVolume() const {
        Float volume = 1._f;
        Vector size = getSize();
        for (int i = 0; i < 3; ++i) {
            volume *= Math::abs(size[i]);
        }
        return volume;
    }

    /// Execute functor for all possible values of vector (with constant stepping)
    template <typename TFunctor>
    void iterate(const Vector& step, TFunctor&& functor) const {
        for (Float x = minBound[X]; x <= maxBound[X]; x += step[X]) {
            for (Float y = minBound[Y]; y <= maxBound[Y]; y += step[Y]) {
                for (Float z = minBound[Z]; z <= maxBound[Z]; z += step[Z]) {
                    functor(Vector(x, y, z));
                }
            }
        }
    }

    /// Execute functor for all possible values of vector (with constant stepping), passing auxiliary indices
    /// together with the vector
    template <typename TFunctor>
    void iterateWithIndices(const Vector& step, TFunctor&& functor) const {
        int i = 0, j = 0, k = 0;
        for (Float z = minBound[Z]; z <= maxBound[Z]; z += step[Z], k++) {
            i = 0;
            j = 0;
            for (Float y = minBound[Y]; y <= maxBound[Y]; y += step[Y], j++) {
                i = 0;
                for (Float x = minBound[X]; x <= maxBound[X]; x += step[X], i++) {
                    functor(Indices(i, j, k), Vector(x, y, z));
                }
            }
        }
    }
};


NAMESPACE_SPH_END