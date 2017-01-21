#pragma once

/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Indices.h"
#include "geometry/Vector.h"
#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

/// Helper object defining three-dimensional interval (box).
class Box {
private:
    Vector minBound;
    Vector maxBound;

public:
    INLINE Box() {
        minBound = Vector(INFTY);
        maxBound = Vector(-INFTY);
    }

    INLINE Box(const Vector& minBound, const Vector& maxBound)
        : minBound(minBound)
        , maxBound(maxBound) {}

    /// Enlarges the box to contain the vector;
    INLINE void extend(const Vector& v) {
        maxBound = max(maxBound, v);
        minBound = min(minBound, v);
    }

    /// Checks if the vector lies inside the box
    INLINE bool contains(const Vector& v) const {
        for (int i = 0; i < 3; ++i) {
            if (v[i] < minBound[i] || v[i] > maxBound[i]) {
                return false;
            }
        }
        return true;
    }

    /// Clamps all components of the vector to fit within the box
    INLINE Vector clamp(const Vector& v) const { return Sph::clamp(v, minBound, maxBound); }

    INLINE const Vector& lower() const { return minBound; }

    INLINE Vector& lower() { return minBound; }

    INLINE const Vector& upper() const { return maxBound; }

    INLINE Vector& upper() { return maxBound; }

    INLINE Vector size() const { return maxBound - minBound; }

    INLINE Float getVolume() const {
        const Vector s = size();
        return s[X] * s[Y] * s[Z];
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

    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const Box& box) {
        stream << box.lower() << box.upper();
        return stream;
    }
};


NAMESPACE_SPH_END
