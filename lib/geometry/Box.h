#pragma once

/// \file Box.h
/// \brief Object representing a three-dimensional axis-aligned box
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "geometry/Indices.h"
#include "geometry/Vector.h"
#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

/// Helper object defining three-dimensional interval (box). Degenerated box (one or more dimensions equal to
/// zero) is a valid state of the object.
class Box {
private:
    Vector minBound = Vector(LARGE);
    Vector maxBound = Vector(-LARGE);

public:
    /// Constructs empty box with negative dimensions. The box in this state will throw assert if member
    /// function are called. Function contains is an exception, it will simply move both lower and upper bound
    /// of the box to the position of new point, resulting in box of zero dimensions.
    /// Another exception is method contains, that simply returns false for all points.
    INLINE Box() = default;

    /// Constructs a box given its 'corners'. Components of minBound must be lower or equal to components of
    /// maxBound, checked by assert.
    INLINE Box(const Vector& minBound, const Vector& maxBound)
        : minBound(minBound)
        , maxBound(maxBound) {
        ASSERT(isValid());
    }

    /// Enlarges the box to contain the vector. If the box already contains given vectors, it is left
    /// unchanged.
    INLINE void extend(const Vector& v) {
        maxBound = max(maxBound, v);
        minBound = min(minBound, v);
    }

    /// Checks if the vector lies inside the box. If the vector lies on the boundary, it is assumed to within
    /// the box and the function returns true.
    INLINE bool contains(const Vector& v) const {
        for (int i = 0; i < 3; ++i) {
            if (v[i] < minBound[i] || v[i] > maxBound[i]) {
                return false;
            }
        }
        return true;
    }

    /// Clamps all components of the vector to fit within the box
    INLINE Vector clamp(const Vector& v) const {
        ASSERT(isValid());
        return Sph::clamp(v, minBound, maxBound);
    }

    /// Returns lower bounds of the box.
    INLINE const Vector& lower() const {
        ASSERT(isValid());
        return minBound;
    }

    /// Returns lower bounds of the box.
    INLINE Vector& lower() {
        ASSERT(isValid());
        return minBound;
    }

    /// Returns upper bounds of the box
    INLINE const Vector& upper() const {
        ASSERT(isValid());
        return maxBound;
    }

    /// Returns upper bounds of the box
    INLINE Vector& upper() {
        ASSERT(isValid());
        return maxBound;
    }

    /// Returns box dimensions.
    INLINE Vector size() const {
        ASSERT(isValid());
        return maxBound - minBound;
    }

    /// Returns the center of the box.
    INLINE Vector center() const {
        ASSERT(isValid());
        return 0.5_f * (minBound + maxBound);
    }

    /// Returns the volume of the box.
    INLINE Float volume() const {
        const Vector s = size();
        return s[X] * s[Y] * s[Z];
    }

    /// Compares two boxes
    INLINE bool operator==(const Box& other) const {
        return minBound == other.minBound && maxBound == other.maxBound;
    }

    /// Splits the box along given coordinate. The splitting plane must pass through the box.
    /// \param dim Dimension, can be X, Y or Z.
    /// \param x Coordinate in given dimension used for the split
    /// \return Two boxes created by the split.
    INLINE Pair<Box> split(const Size dim, const Float x) const {
        ASSERT(dim < 3);
        ASSERT(x >= minBound[dim] && x <= maxBound[dim]);
        Box b1 = *this, b2 = *this;
        b1.maxBound[dim] = x;
        b2.minBound[dim] = x;
        return { b1, b2 };
    }


    /// Execute functor for all possible values of vector (with constant stepping)
    template <typename TFunctor>
    void iterate(const Vector& step, TFunctor&& functor) const {
        ASSERT(isValid());
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
        ASSERT(isValid());
        Size i = 0, j = 0, k = 0;
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

    friend std::ostream& operator<<(std::ostream& stream, const Box& box) {
        ASSERT(box.isValid());
        stream << box.lower() << box.upper();
        return stream;
    }

private:
    bool isValid() const {
        return minElement(maxBound - minBound) >= 0._f;
    }
};


NAMESPACE_SPH_END
