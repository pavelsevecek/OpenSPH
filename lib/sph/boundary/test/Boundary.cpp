#include "sph/boundary/Boundary.h"
#include "catch.hpp"
#include "math/rng/VectorRng.h"
#include "objects/containers/ArrayUtils.h"
#include <iostream>

using namespace Sph;

/// testing domain, simply a wall x = 0 keeping particles in positive values of x. Only methods necessary for
/// ghost particles are implemented.
class WallDomain : public Abstract::Domain {
public:
    WallDomain()
        : Abstract::Domain(Vector(0._f), 1._f) {}

    virtual Float getVolume() const override { NOT_IMPLEMENTED; }

    virtual bool isInside(const Vector& UNUSED(v)) const override { NOT_IMPLEMENTED; }

    virtual void getSubset(const ArrayView<Vector> UNUSED(vs),
        Array<int>& UNUSED(output),
        const SubsetType UNUSED(type)) const override {
        NOT_IMPLEMENTED;
    }

    virtual void getDistanceToBoundary(ArrayView<Vector> vs, Array<Float>& distances) const {
        distances.clear();
        for (const Vector& v : vs) {
            distances.push(v[X]);
        }
    }

    virtual void project(ArrayView<Vector> vs, ArrayView<int> indices = nullptr) const {
        if (indices) {
            for (int i : indices) {
                vs[i][X] = Math::max(0._f, vs[i][X]);
            }
        } else {
            for (Vector& v : vs) {
                v[X] = Math::max(0._f, v[X]);
            }
        }
    }

    virtual void invert(ArrayView<Vector> vs, ArrayView<int> indices = nullptr) const {
        if (indices) {
            for (int i : indices) {
                vs[i][X] *= -1;
            }
        } else {
            for (Vector& v : vs) {
                v[X] *= -1;
            }
        }
    }
};

TEST_CASE("GhostParticles wall", "[boundary]") {
    // default kernel = M4, radius = 2
    GhostParticles boundaryConditions(std::make_unique<WallDomain>(), GLOBAL_SETTINGS);
    Storage storage;
    // Create few particles. Particles with x < 2 will create corresponding ghost particle.
    storage.emplace<Vector, OrderEnum::SECOND_ORDER>(QuantityKey::POSITIONS,
        Array<Vector>{ Vector(1.5_f, 1._f, 3._f, 1._f), // has ghost
            Vector(0.5_f, 2._f, -1._f, 1._f),           // has ghost
            Vector(-1._f, 2._f, 1._f, 1._f),            // negative - will be projected, + ghost
            Vector(0._f, 0._f, 0._f, 1._f),             // has ghost
            Vector(5._f, 1._f, 1._f, 1._f),             // does not have ghost
            Vector(1._f, 1._f, 1._f, 1._f),             // has ghost
            Vector(2.5_f, 0._f, 5._f, 1._f) });         // does not have ghost
    ArrayView<Vector> r, v, dv;
    tieToArray(r, v, dv) = storage.getAll<Vector>(QuantityKey::POSITIONS);
    // add some velocities, the x-coordinate of the corresponding ghost should be inverted by the boundary
    // conditions
    v[0] = Vector(-1._f, 1._f, 1._f);
    v[1] = Vector(0._f, 2._f, 1._f);
    v[2] = Vector(1._f, 0._f, -3._f);
    // add scalar quantity, should be simply copied onto ghosts
    storage.emplace<Float, OrderEnum::FIRST_ORDER>(
        QuantityKey::DENSITY, Array<Float>{ 3._f, 5._f, 2._f, 1._f, 3._f, 4._f, 10._f });

    boundaryConditions.apply(storage);
    tieToArray(r, v, dv) = storage.getAll<Vector>(QuantityKey::POSITIONS);
    REQUIRE(makeArray(r.size(), v.size(), dv.size()) == makeArray(12, 12, 12));
    REQUIRE(r[7] == Vector(-1.5_f, 1._f, 3._f));
    REQUIRE(r[8] == Vector(-0.5_f, 2._f, -1._f));
    REQUIRE(r[9] == Vector(0._f, 2._f, 1._f));
    REQUIRE(r[10] == Vector(0._f, 0._f, 0._f));
    REQUIRE(r[11] == Vector(-1._f, 1._f, 1._f));

    REQUIRE(Math::almostEqual(v[7], Vector(1._f, 1._f, 1._f), 1.e-3_f));
    REQUIRE(Math::almostEqual(v[8], Vector(0._f, 2._f, 1._f), 1.e-3_f));
    REQUIRE(Math::almostEqual(v[9], Vector(-1._f, 0._f, -3._f), 1.e-3_f));

    ArrayView<Float> rho = storage.getValue<Float>(QuantityKey::DENSITY);
    REQUIRE(rho[7] == 3._f);
    REQUIRE(rho[8] == 5._f);
    REQUIRE(rho[9] == 2._f);
    REQUIRE(rho[10] == 1._f);
    REQUIRE(rho[11] == 4._f);

    // subsequent calls shouldn't change result
    boundaryConditions.apply(storage);
    tieToArray(r, v, dv) = storage.getAll<Vector>(QuantityKey::POSITIONS);
    REQUIRE(makeArray(r.size(), v.size(), dv.size()) == makeArray(12, 12, 12));
    REQUIRE(r[7] == Vector(-1.5_f, 1._f, 3._f));
    REQUIRE(Math::almostEqual(v[7], Vector(1._f, 1._f, 1._f), 1.e-3_f));
    rho = storage.getValue<Float>(QuantityKey::DENSITY);
    REQUIRE(rho[7] == 3._f);
}

TEST_CASE("GhostParticles Sphere", "[boundary]") {
    Storage storage;
    Array<Vector> particles;
    for (Float phi = 0._f; phi < 2._f * Math::PI; phi += 0.1_f) {
        for (Float theta = 0._f; theta < Math::PI; theta += 0.1_f) {
            Vector v = spherical(1.9_f, theta, phi);
            v[H] = 0.1_f;
            particles.push(v);
        }
    }
    storage.emplace<Vector, OrderEnum::SECOND_ORDER>(QuantityKey::POSITIONS, std::move(particles));
    ArrayView<Vector> r, v, dv;
    tieToArray(r, v, dv) = storage.getAll<Vector>(QuantityKey::POSITIONS);
    VectorRng<UniformRng> rng;
    // randomize velocities
    for (Vector& q : v) {
        q = rng();
    }

    const int ghostIdx = r.size();
    GhostParticles boundaryConditions(std::make_unique<SphericalDomain>(Vector(0._f), 2._f), GLOBAL_SETTINGS);
    boundaryConditions.apply(storage);
    tieToArray(r, v, dv) = storage.getAll<Vector>(QuantityKey::POSITIONS);
    REQUIRE(r.size() == 2 * ghostIdx); // ghost for each particle
    bool allSymmetric = true;
    for (int i = 0; i < ghostIdx; ++i) {
        Vector normalized;
        Float length;
        tieToTuple(normalized, length) = getNormalizedWithLength(r[ghostIdx + i]);
        if (!Math::almostEqual(length, 2.1_f)) {
            std::cout << "Incorrect position of ghost: " << length << std::endl;
            allSymmetric = false;
            break;
        }
        if (!Math::almostEqual(normalized, getNormalized(r[i]))) {
            std::cout << "Incorrect position of ghost: " << normalized << std::endl;
            allSymmetric = false;
            break;
        }
        // check that velocities are symmetric == their perpendicular component is inverted
        const Float vPerp = dot(v[i], normalized);
        const Float vgPerp = dot(v[ghostIdx + i], normalized);
        if (!Math::almostEqual(vPerp, -vgPerp, 1.e-5_f)) {
            std::cout << "Perpendicular component not inverted: " << vPerp << "  " << vgPerp << std::endl;
            allSymmetric = false;
            break;
        }
        // parallel component should be equal
        const Vector vPar = v[i] - normalized * dot(v[i], normalized);
        const Vector vgPar = v[ghostIdx + i] - normalized * dot(v[ghostIdx + i], normalized);
        if (!Math::almostEqual(vPar, vgPar, 1.e-5_f)) {
            std::cout << "Parallel component not copied: " << vPar << "  " << vgPar << std::endl;
            allSymmetric = false;
            break;
        }
    }
    REQUIRE(allSymmetric);
}
