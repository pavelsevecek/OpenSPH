#pragma once

/// \file ScriptUtils.h
/// \brief Utility functions and classes exposed to the embedded scripting language.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "post/Analysis.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "system/Factory.h"
#include "thread/Scheduler.h"

#ifdef SPH_USE_CHAISCRIPT

#include <chaiscript/chaiscript.hpp>
#include <numeric>

#ifdef SPH_WIN
#if defined(min)
#undef min
#endif
#if defined(max)
#undef max
#endif
#endif

NAMESPACE_SPH_BEGIN

namespace Chai {

struct Vec3 {
    Float x, y, z;

    Vec3() = default;

    Vec3(const Float x, const Float y, const Float z)
        : x(x)
        , y(y)
        , z(z) {}

    Vec3(const Vec3& other) {
        *this = other;
    }

    Vec3(const Sph::Vector& v)
        : x(v[X])
        , y(v[Y])
        , z(v[Z]) {}

    operator Sph::Vector() const {
        return Sph::Vector(x, y, z);
    }

    Vec3& operator=(const Vec3& other) {
        x = other.x;
        y = other.y;
        z = other.z;
        return *this;
    }

    Vec3 operator+(const Vec3& other) const {
        return { x + other.x, y + other.y, z + other.z };
    }
    Vec3 operator-(const Vec3& other) const {
        return { x - other.x, y - other.y, z - other.z };
    }
    Vec3 operator*(const Float f) const {
        return { x * f, y * f, z * f };
    }
    Vec3 operator/(const Float f) const {
        return { x / f, y / f, z / f };
    }
    Vec3& operator+=(const Vec3& other) {
        *this = *this + other;
        return *this;
    }
};

INLINE Vec3 operator*(const Float f, const Vec3& v) {
    return v * f;
}

INLINE Float dot(const Vec3& v1, const Vec3& v2) {
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}
INLINE Vec3 cross(const Vec3& v1, const Vec3& v2) {
    return Vec3{ v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x };
}
INLINE Float length(const Vec3& v) {
    return sqrt(dot(v, v));
}
INLINE Vec3 min(const Vec3& v1, const Vec3& v2) {
    return { Sph::min(v1.x, v2.x), Sph::min(v1.z, v2.z), Sph::min(v1.z, v2.z) };
}
INLINE Vec3 max(const Vec3& v1, const Vec3& v2) {
    return { Sph::max(v1.x, v2.x), Sph::max(v1.z, v2.z), Sph::max(v1.z, v2.z) };
}
INLINE Vec3 normalized(const Vec3& v) {
    return v / length(v);
}

class Box3 {
    Vec3 lower, upper;

public:
    Box3()
        : lower(INFTY, INFTY, INFTY)
        , upper(-INFTY, -INFTY, -INFTY) {}

    Box3(const Vec3& lower, const Vec3& upper)
        : lower(lower)
        , upper(upper) {}

    Box3(const Box3& other) {
        *this = other;
    }

    Box3& operator=(const Box3& other) {
        lower = other.lower;
        upper = other.upper;
        return *this;
    }

    Vec3 size() const {
        return upper - lower;
    }

    void extend(const Vec3& pos) {
        lower = min(lower, pos);
        upper = max(upper, pos);
    }
};

/// \brief Wrapper of \ref Storage that allows modifications from Chaiscript.
class Particles {
private:
    /// Stores all particle data, either created in the constructor or provided on input
    Storage* storage = nullptr;

    /// Whether the storage is owned or only referenced
    bool owns = true;

    /// Overrides specified by the script.
    std::vector<Vec3> positions, velocities, accelerations;
    std::vector<Float> masses, energies, densities, radii;

public:
    Particles() = default;

    Particles(const Size particleCnt);

    ~Particles();

    void bindToStorage(Storage& input);

    const Storage& store() const;

    Size getParticleCnt() const;

    void resize(const Size particleCnt);

    std::vector<Float>& getMasses();

    std::vector<Float>& getEnergies();

    std::vector<Float>& getDensities();

    std::vector<Float>& getRadii();

    std::vector<Vec3>& getPositions();

    std::vector<Vec3>& getVelocities();

    std::vector<Vec3>& getAccelerations();

    Box3 getBoundingBox() const;

    Float getTotalMass() const;

    Vec3 getCenterOfMass() const;

    Vec3 getTotalMomentum() const;

    Vec3 getTotalAngularMomentum() const;

    Vec3 getAngularFrequency() const;

    void merge(Particles& other);
};

/// \brief Adds custom functions and classes to the Chaiscript engine.
void registerBindings(chaiscript::ChaiScript& chai);

} // namespace Chai

NAMESPACE_SPH_END

#endif
