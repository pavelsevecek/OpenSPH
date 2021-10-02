#include "sph/boundary/Boundary.h"
#include "catch.hpp"
#include "math/rng/VectorRng.h"
#include "objects/geometry/Domain.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "sph/Materials.h"
#include "sph/initial/Distribution.h"
#include "sph/initial/Initial.h"
#include "system/Factory.h"
#include "system/Settings.h"
#include "tests/Approx.h"
#include "thread/Pool.h"
#include "timestepping/ISolver.h"
#include "utils/SequenceTest.h"

using namespace Sph;

/// testing domain, simply a wall x = 0 keeping particles in positive values of x. Only methods necessary for
/// ghost particles are implemented.
class WallDomain : public IDomain {
public:
    virtual Vector getCenter() const override {
        NOT_IMPLEMENTED
    }

    virtual Float getVolume() const override {
        NOT_IMPLEMENTED;
    }

    virtual Float getSurfaceArea() const override {
        NOT_IMPLEMENTED;
    }

    virtual Box getBoundingBox() const override {
        NOT_IMPLEMENTED;
    }

    virtual bool contains(const Vector& UNUSED(v)) const override {
        NOT_IMPLEMENTED;
    }

    virtual void getSubset(ArrayView<const Vector> UNUSED(vs),
        Array<Size>& UNUSED(output),
        const SubsetType UNUSED(type)) const override {
        NOT_IMPLEMENTED;
    }

    virtual void getDistanceToBoundary(ArrayView<const Vector> vs, Array<Float>& distances) const override {
        distances.clear();
        for (const Vector& v : vs) {
            distances.push(v[X]);
        }
    }

    virtual void project(ArrayView<Vector> vs, Optional<ArrayView<Size>> indices = NOTHING) const override {
        if (indices) {
            for (Size i : indices.value()) {
                vs[i][X] = max(0._f, vs[i][X]);
            }
        } else {
            for (Vector& v : vs) {
                v[X] = max(0._f, v[X]);
            }
        }
    }

    virtual void addGhosts(ArrayView<const Vector> vs,
        Array<Ghost>& ghosts,
        const Float radius,
        const Float eps) const override {
        ghosts.clear();
        Size idx = 0;
        for (const Vector& v : vs) {
            if (abs(v[X]) < radius * v[H]) {
                Vector ghost(-max(v[X], eps * v[H]), v[Y], v[Z]);
                ghosts.push(Ghost{ ghost, idx });
            }
            idx++;
        }
    }
};

TEST_CASE("GhostParticles wall", "[boundary]") {
    // default kernel = M4, radius = 2
    const Float minDist = 0.1_f; // minimal distance of ghost
    RunSettings settings;
    settings.set(RunSettingsId::DOMAIN_GHOST_MIN_DIST, minDist);
    GhostParticles bc(makeAuto<WallDomain>(), settings);
    Storage storage;
    // Create few particles. Particles with x < 2 will create corresponding ghost particle.
    storage.insert<Vector>(QuantityId::POSITION,
        OrderEnum::SECOND,
        Array<Vector>{ Vector(1.5_f, 1._f, 3._f, 1._f), // has ghost
            Vector(0.5_f, 2._f, -1._f, 1._f),           // has ghost
            Vector(-1._f, 2._f, 1._f, 1._f),            // negative - will be projected, + ghost
            Vector(0._f, 0._f, 0._f, 1._f),             // lies on the boundary, has ghost
            Vector(5._f, 1._f, 1._f, 1._f),             // does not have ghost
            Vector(1._f, 1._f, 1._f, 1._f),             // has ghost
            Vector(2.5_f, 0._f, 5._f, 1._f) });         // does not have ghost
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    // add some velocities, the x-coordinate of the corresponding ghost should be inverted by the boundary
    // conditions
    v[0] = Vector(-1._f, 1._f, 1._f);
    v[1] = Vector(0._f, 2._f, 1._f);
    v[2] = Vector(1._f, 0._f, -3._f);
    // add scalar quantity, should be simply copied onto ghosts
    storage.insert<Float>(
        QuantityId::DENSITY, OrderEnum::FIRST, Array<Float>{ 3._f, 5._f, 2._f, 1._f, 3._f, 4._f, 10._f });

    bc.initialize(storage);
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    REQUIRE(makeArray(r.size(), v.size(), dv.size()) == makeArray(12u, 12u, 12u));
    REQUIRE(r[7] == Vector(-1.5_f, 1._f, 3._f));
    REQUIRE(r[8] == Vector(-0.5_f, 2._f, -1._f));
    REQUIRE(r[9] == Vector(-minDist, 2._f, 1._f));
    REQUIRE(r[10] == Vector(-minDist, 0._f, 0._f));
    REQUIRE(r[11] == Vector(-1._f, 1._f, 1._f));

    REQUIRE(v[7] == approx(Vector(1._f, 1._f, 1._f), 1.e-3_f));
    REQUIRE(v[8] == approx(Vector(0._f, 2._f, 1._f), 1.e-3_f));
    REQUIRE(v[9] == approx(Vector(-1._f, 0._f, -3._f), 1.e-3_f));

    ArrayView<Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
    REQUIRE(rho[7] == 3._f);
    REQUIRE(rho[8] == 5._f);
    REQUIRE(rho[9] == 2._f);
    REQUIRE(rho[10] == 1._f);
    REQUIRE(rho[11] == 4._f);
    bc.finalize(storage);

    // subsequent calls shouldn't change result
    bc.initialize(storage);
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    REQUIRE(makeArray(r.size(), v.size(), dv.size()) == makeArray(12u, 12u, 12u));
    REQUIRE(r[7] == Vector(-1.5_f, 1._f, 3._f));
    REQUIRE(v[7] == approx(Vector(1._f, 1._f, 1._f), 1.e-3_f));
    rho = storage.getValue<Float>(QuantityId::DENSITY);
    REQUIRE(rho[7] == 3._f);
}

TEST_CASE("GhostParticles Sphere", "[boundary]") {
    Storage storage;
    Array<Vector> particles;
    for (Float phi = 0._f; phi < 2._f * PI; phi += 0.1_f) {
        for (Float theta = 0._f; theta < PI; theta += 0.1_f) {
            Vector v = sphericalToCartesian(1.9_f, theta, phi);
            v[H] = 0.1_f;
            particles.push(v);
        }
    }
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(particles));
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    VectorRng<UniformRng> rng;
    // randomize velocities
    for (Vector& q : v) {
        q = rng();
    }

    const Size ghostIdx = r.size();
    GhostParticles bc(makeAuto<SphericalDomain>(Vector(0._f), 2._f), RunSettings::getDefaults());
    bc.initialize(storage);
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    REQUIRE(r.size() == 2 * ghostIdx); // ghost for each particle

    auto test = [&](const Size i) -> Outcome {
        Vector normalized;
        Float length;
        tieToTuple(normalized, length) = getNormalizedWithLength(r[ghostIdx + i]);
        if (length != approx(2.1_f)) {
            return makeFailed("Incorrect position of ghost: {}", length);
        }
        if (normalized != approx(getNormalized(r[i]))) {
            return makeFailed("Incorrect position of ghost: {}", normalized);
        }
        // check that velocities are symmetric == their perpendicular component is inverted
        const Float vPerp = dot(v[i], normalized);
        const Float vgPerp = dot(v[ghostIdx + i], normalized);
        if (vPerp != approx(-vgPerp, 1.e-5_f)) {
            return makeFailed("Perpendicular component not inverted: {} == -{}", vPerp, vgPerp);
        }
        // parallel component should be equal
        const Vector vPar = v[i] - normalized * dot(v[i], normalized);
        const Vector vgPar = v[ghostIdx + i] - normalized * dot(v[ghostIdx + i], normalized);
        if (vPar != approx(vgPar, 1.e-5_f)) {
            return makeFailed("Parallel component not copied: {} == {}", vPar, vgPar);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, ghostIdx);
}

TEST_CASE("GhostParticles Sphere Projection", "[boundary]") {
    Storage storage;
    Array<Vector> particles;
    // two spherical layers of particles
    for (Float phi = 0._f; phi < 2._f * PI; phi += 0.1_f) {
        for (Float theta = 0._f; theta < PI; theta += 0.1_f) {
            Vector v = sphericalToCartesian(1.9_f, theta, phi);
            v[H] = 0.1_f;
            particles.push(v);
            v = sphericalToCartesian(0.9_f, theta, phi);
            v[H] = 0.1_f;
            particles.push(v);
        }
    }
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(particles));
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    const Size ghostIdx = r.size();
    const Size halfSize = ghostIdx >> 1;
    GhostParticles bc(makeAuto<SphericalDomain>(Vector(0._f), 2._f), RunSettings::getDefaults());
    bc.initialize(storage);
    r = storage.getValue<Vector>(QuantityId::POSITION);
    REQUIRE(r.size() == halfSize * 3); // only layer with r=1.9 creates ghost particles

    auto test = [&](const Size i) -> Outcome {
        if (i % 2 == 0) {
            if (getLength(r[i]) != approx(1.9_f)) {
                return makeFailed("Invalid particle position: {} == 1.9", getLength(r[i]));
            }
        } else {
            if (getLength(r[i]) != approx(0.9_f)) {
                return makeFailed("Invalid particle position: {} == 0.9", getLength(r[i]));
            }
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, ghostIdx);
}

TEST_CASE("GhostParticles empty", "[boundary]") {
    Storage storage;
    Array<Vector> particles;
    particles.push(Vector(1._f, 0._f, 0._f, 0.1_f));
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(particles));
    GhostParticles bc(makeAuto<SphericalDomain>(Vector(0._f), 2._f), RunSettings::getDefaults());
    bc.initialize(storage);
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    REQUIRE(r.size() == 1);
    REQUIRE(r[0] == Vector(1._f, 0._f, 0._f));
}

TEST_CASE("GhostParticles with material", "[boundary]") {
    Storage storage(getMaterial(MaterialEnum::BASALT));
    AutoPtr<SphericalDomain> domain = makeAuto<SphericalDomain>(Vector(0._f), 1.5_f);
    InitialConditions ic(RunSettings::getDefaults());
    BodySettings body;
    body.set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::RANDOM);
    body.set(BodySettingsId::PARTICLE_COUNT, 1000);
    ic.addMonolithicBody(storage, *domain, body);
    REQUIRE(storage.getParticleCnt() == 1000);
    REQUIRE(storage.getMaterialCnt() == 1);

    GhostParticles bc(std::move(domain), 2._f, 0.1_f);
    bc.initialize(storage);
    REQUIRE(storage.getParticleCnt() > 1100);
    REQUIRE(storage.getMaterialCnt() == 1);

    bc.finalize(storage);
    REQUIRE(storage.getParticleCnt() == 1000);
}

static void createSolverQuantities(Storage& storage) {
    RunSettings settings;
    AutoPtr<ISolver> solver = Factory::getSolver(*ThreadPool::getGlobalInstance(), settings);
    for (Size i = 0; i < storage.getMaterialCnt(); ++i) {
        solver->create(storage, storage.getMaterial(i));
    }
}

TEST_CASE("FrozenParticles by flag", "[boundary]") {
    Storage storage;
    InitialConditions conds(RunSettings::getDefaults());
    BodySettings settings;
    settings.set(BodySettingsId::PARTICLE_COUNT, 100);
    conds.addMonolithicBody(storage, SphericalDomain(Vector(0._f), 1._f), settings);
    const Size size0 = storage.getParticleCnt();
    conds.addMonolithicBody(storage, SphericalDomain(Vector(3._f, 0._f, 0._f), 1._f), settings);
    createSolverQuantities(storage);

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    ArrayView<Float> u, du;
    tie(u, du) = storage.getAll<Float>(QuantityId::ENERGY);
    // fill with some nonzero derivatives
    const Vector v0 = Vector(5._f, 3._f, 1._f);
    const Vector dv0 = Vector(3._f, 3._f, -1._f);
    const Float du0 = 12._f;
    auto setup = [&] {
        for (Size i = 0; i < r.size(); ++i) {
            v[i] = v0;
            dv[i] = dv0;
            du[i] = du0;
        }
    };

    setup();
    FrozenParticles boundaryConditions;
    boundaryConditions.freeze(1);
    boundaryConditions.finalize(storage);

    auto test1 = [&](const Size i) -> Outcome {
        if (i < size0 && (v[i] != v0 || dv[i] != dv0 || du[i] != du0)) {
            // clang-format off
            return makeFailed("Incorrect particles frozen:\n v: {} == {}\n dv: {} == {}\n du: {} == {}",
                              v[i], v0, dv[i], dv0, du[i], du0);
            // clang-format on
        }
        if (i >= size0 && (v[i] != v0 || dv[i] != Vector(0._f) || du[i] != 0._f)) {
            // clang-format off
            return makeFailed("Particles didn't freeze correctly:\n v: {} == {}\n dv: {} == {}\n du: {} == {}",
                              v[i], v0, dv[i], Vector(0._f), du[i], 0._f);
            // clang-format on
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test1, 0, r.size());

    boundaryConditions.freeze(0);
    setup();
    boundaryConditions.finalize(storage);
    auto test2 = [&](const Size i) -> Outcome {
        if (v[i] != v0 || dv[i] != Vector(0._f) || du[i] != 0._f) {
            // clang-format off
            return makeFailed("Nonzero derivatives after freezing:\n v: {} == {}\n dv: {} == {}\n du: {} == {}",
                              v[i], v0, dv[i], Vector(0._f), du[i], 0._f);
            // clang-format on
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test2, 0, r.size());

    boundaryConditions.thaw(1);
    setup();
    boundaryConditions.finalize(storage);
    auto test3 = [&](const Size i) -> Outcome {
        if (i >= size0 && (v[i] != v0 || dv[i] != dv0 || du[i] != du0)) {
            // clang-format off
            return makeFailed("Incorrect particles frozen:\n v: {} == {}\n dv: {} == {}\n du: {} == {}",
                              v[i], v0, dv[i], dv0, du[i], du0);
            // clang-format on
        }
        if (i < size0 && (v[i] != v0 || dv[i] != Vector(0._f) || du[i] != 0._f)) {
            // clang-format off
            return makeFailed("Particles didn't freeze correctly:\n v: {} == {}\n dv: {} == {}\n du: {} == {}",
                              v[i], v0, dv[i], Vector(0._f), du[i], 0._f);
            // clang-format on
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test3, 0, r.size());
}

TEST_CASE("FrozenParticles by distance", "[boundary]") {
    Storage storage;
    InitialConditions conds(RunSettings::getDefaults());
    BodySettings settings;
    settings.set(BodySettingsId::PARTICLE_COUNT, 1000);
    conds.addMonolithicBody(storage, SphericalDomain(Vector(0._f), 1.5_f), settings);
    createSolverQuantities(storage);

    const Float radius = 2._f;
    FrozenParticles boundaryConditions(makeAuto<SphericalDomain>(Vector(0._f), 1._f), radius);

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    const Float h = r[0][H];
    ArrayView<Float> u, du;
    tie(u, du) = storage.getAll<Float>(QuantityId::ENERGY);
    // fill with some nonzero derivatives
    const Vector v0 = Vector(5._f, 3._f, 1._f);
    const Vector dv0 = Vector(3._f, 3._f, -1._f);
    const Float du0 = 12._f;
    for (Size i = 0; i < r.size(); ++i) {
        v[i] = v0;
        dv[i] = dv0;
        du[i] = du0;
    }

    boundaryConditions.finalize(storage);
    REQUIRE(storage.getParticleCnt() == r.size()); // sanity check that we don't add or lose particles

    auto test = [&](const Size i) -> Outcome {
        const Float dist = getLength(r[i]);
        if (dist > 1._f + EPS) {
            return makeFailed("Particle not projected inside the domain:\n dist == {}", dist);
        }
        if (dist > 1._f - radius * h) {
            // should be frozen
            if (v[i] != v0 || dv[i] != Vector(0._f) || du[i] != 0._f) {
                // clang-format off
                return makeFailed("Particles didn't freeze correctly:\n v: {} == {}\n dv: {} == {}\n du: {} == {}",
                                   v[i], v0, dv[i], Vector(0._f), du[i], 0._f);
                // clang-format on
            } else {
                return SUCCESS;
            }
        } else {
            if (v[i] != v0 || dv[i] != dv0 || du[i] != du0) {
                // clang-format off
                return makeFailed("Incorrect particles frozen: \n v: {} == {}\n dv: {} == {}\n du: {} == {}",
                                   v[i], v0, dv[i], dv0, du[i], du0);
                // clang-format on
            } else {
                return SUCCESS;
            }
        }
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}

/// \todo uncomment
/*
TEST_CASE("WindTunnel generating particles", "[boundary]") {
    CylindricalDomain domain(Vector(0._f, 0._f, 2._f), 1._f, 4._f, true); // z in [0, 4]
    Storage storage1;
    HexagonalPacking distribution(EMPTY_FLAGS);
    storage1.insert<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND, distribution.generate(1000, domain));
    Storage storage2; // will contain subset of particles with z in [0, 2]
    Array<Vector> r2;
    Array<Vector>& r_ref = storage1.getValue<Vector>(QuantityId::POSITIONS);
    for (Size i = 0; i < r_ref.size(); ++i) {
        if (r_ref[i][Z] < 2._f) {
            r2.push(r_ref[i]);
        }
    }
    storage2.insert<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND, std::move(r2));
    // some other quantities for a ride
    storage2.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, 5._f);
    storage2.insert<Size>(QuantityId::MATERIAL_IDX, OrderEnum::ZERO, 5);
    WindTunnel boundary(makeAuto<CylindricalDomain>(domain), 2._f);
    const Size size1 = r_ref.size();
    boundary.apply(storage1); // whole domain filled with particles, no particle should be added nor remoted
    REQUIRE(r_ref.size() == size1);

    const Size size2 = storage2.getParticleCnt();
    REQUIRE(size2 > 400); // sanity check
    boundary.apply(storage2);
    REQUIRE(storage2.isValid());
    Array<Vector>& r = storage2.getValue<Vector>(QuantityId::POSITIONS);
    REQUIRE(r.size() > size2); // particles have been added
    REQUIRE(r.size() < r_ref.size());
    // sort both arrays lexicographically so that particles with same indices match
    std::sort(r.begin(), r.end(), [](Vector v1, Vector v2) { return lexicographicalLess(v1, v2); });
    std::sort(r_ref.begin(), r_ref.end(), [](Vector v1, Vector v2) { return lexicographicalLess(v1, v2); });
    auto test = [&](const Size i) {
        if (r[i] != approx(r_ref[i])) {
            return makeFailed("Invalid positions: \n", r[i], " == ", r_ref[i]);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}
*/
