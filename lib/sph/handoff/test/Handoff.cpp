#include "sph/handoff/Handoff.h"
#include "catch.hpp"
#include "objects/geometry/Domain.h"
#include "physics/Integrals.h"
#include "sph/initial/Distribution.h"
#include "sph/initial/Initial.h"
#include "tests/Approx.h"
#include "thread/Scheduler.h"
#include "utils/SequenceTest.h"

using namespace Sph;

TEST_CASE("Regenerate largest remnant", "[handoff]") {
    RunSettings settings;
    BodySettings body;
    InitialConditions ic(SEQUENTIAL, settings);

    Storage sph;
    // so that we have the exact number of particles
    body.set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::RANDOM);
    body.set(BodySettingsId::PARTICLE_COUNT, 100);
    ic.addMonolithicBody(sph, SphericalDomain(Vector(30._f, 0._f, 0._f), 3._f), body)
        .addVelocity(Vector(4._f, 3._f, 2._f));

    body.set(BodySettingsId::PARTICLE_COUNT, 1000);
    ic.addMonolithicBody(sph, SphericalDomain(Vector(0._f), 10._f), body);
    // sanity check
    REQUIRE(sph.getParticleCnt() == 1100);
    Array<Vector> r_sph = sph.getValue<Vector>(QuantityId::POSITION).clone();
    Array<Vector> v_sph = sph.getDt<Vector>(QuantityId::POSITION).clone();
    Array<Float> m_sph = sph.getValue<Float>(QuantityId::MASS).clone();


    HandoffParams params;
    params.largestRemnant.emplace();
    params.largestRemnant->particleOverride = 350;
    params.largestRemnant->distribution = makeAuto<RandomDistribution>(1234);

    Storage nbody = convertSphToSpheres(sph, settings, params);
    REQUIRE(nbody.has(QuantityId::POSITION));
    REQUIRE(nbody.has(QuantityId::MASS));
    REQUIRE(nbody.getParticleCnt() == 450);

    ArrayView<const Vector> r_nbody = nbody.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Vector> v_nbody = nbody.getDt<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m_nbody = nbody.getValue<Float>(QuantityId::MASS);

    // total mass should be +- the same
    REQUIRE(std::accumulate(m_sph.begin(), m_sph.end(), 0._f) ==
            approx(std::accumulate(m_nbody.begin(), m_nbody.end(), 0._f)));


    // first 100 particles should have *exactly* the same masses, positions and velocities
    // (it's not really required to preserve the order in handoff, but the current implementation does, which
    // simplifies testing)
    auto test1 = [&](Size i) -> Outcome {
        return makeOutcome(
            r_nbody[i] == r_sph[i] && v_nbody[i] == v_sph[i] && m_nbody[i] == m_sph[i], "Particles differ");
    };
    REQUIRE_SEQUENCE(test1, 0, 100);

    // the next particle is already different (LR has been re-generated)
    REQUIRE(r_nbody[100] != r_sph[100]);
    REQUIRE(m_nbody[100] > m_sph[100]); // fewer particles -> more massive
    // velocity has been preserved though
    REQUIRE(v_nbody[100] == v_sph[100]);

    // all particles are inside the original sphere
    auto test2 = [&](Size i) -> Outcome {
        return makeOutcome(
            getLength(r_nbody[i]) < 12._f, "Particle outside the sphere: r = ", getLength(r_nbody[i]));
    };
    REQUIRE_SEQUENCE(test2, 100, r_nbody.size());
}

TEST_CASE("Handoff conserves angular momentum", "[handoff]") {
    RunSettings settings;
    BodySettings body;
    InitialConditions ic(SEQUENTIAL, settings);

    Storage sph;
    body.set(BodySettingsId::PARTICLE_COUNT, 15000);
    ic.addMonolithicBody(sph, SphericalDomain(Vector(0._f), 10._f), body)
        .addRotation(Vector(-7._f, 8._f, 25._f), Vector(0._f));

    TotalAngularMomentum angMom;
    const Vector L_sph = angMom.evaluate(sph);


    HandoffParams params;
    params.largestRemnant.emplace();
    params.largestRemnant->particleOverride = 5000;
    params.largestRemnant->distribution = makeAuto<HexagonalPacking>(HexagonalPacking::Options::CENTER);

    Storage nbody = convertSphToSpheres(sph, settings, params);
    const Vector L_nbody = angMom.evaluate(nbody);
    REQUIRE(L_sph == approx(L_nbody, 0.1)); /// \todo can we improve this?
}
