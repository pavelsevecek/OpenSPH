#include "sph/initial/Initial.h"
#include "catch.hpp"
#include "io/Column.h"
#include "io/Output.h"
#include "objects/geometry/Domain.h"
#include "objects/utility/ArrayUtils.h"
#include "physics/Integrals.h"
#include "quantities/Iterate.h"
#include "quantities/Storage.h"
#include "system/Factory.h"
#include "system/Settings.h"
#include "tests/Approx.h"
#include "thread/Pool.h"
#include "timestepping/ISolver.h"
#include "utils/SequenceTest.h"

using namespace Sph;

TEST_CASE("Initial addBody", "[initial]") {
    BodySettings bodySettings;
    bodySettings.set(BodySettingsId::PARTICLE_COUNT, 100);
    BlockDomain domain(Vector(0._f), Vector(1._f));
    Storage storage;
    InitialConditions conds(RunSettings::getDefaults());
    conds.addMonolithicBody(storage, domain, bodySettings);
    AutoPtr<ISolver> solver =
        Factory::getSolver(*ThreadPool::getGlobalInstance(), RunSettings::getDefaults());
    solver->create(storage, storage.getMaterial(0));

    const Size size = storage.getValue<Vector>(QuantityId::POSITION).size();
    REQUIRE(size >= 80);
    REQUIRE(size <= 120);
    iterate<VisitorEnum::ALL_BUFFERS>(storage, [size](const auto& array) { REQUIRE(array.size() == size); });

    ArrayView<Float> rhos, us, drhos, dus;
    tie(rhos, drhos) = storage.getAll<Float>(QuantityId::DENSITY);
    tie(us, dus) = storage.getAll<Float>(QuantityId::ENERGY);
    bool result = areAllMatching(
        rhos, [&](const Float f) { return f == bodySettings.get<Float>(BodySettingsId::DENSITY); });
    REQUIRE(result);

    result = areAllMatching(drhos, [](const Float f) {
        return f == 0._f; // zero density derivative
    });
    REQUIRE(result);
    result = areAllMatching(
        us, [&](const Float f) { return f == bodySettings.get<Float>(BodySettingsId::ENERGY); });
    REQUIRE(result);
    result = areAllMatching(dus, [](const Float f) {
        return f == 0._f; // zero energy derivative
    });
    REQUIRE(result);

    ArrayView<Float> ms = storage.getValue<Float>(QuantityId::MASS);
    Float totalM = 0._f;
    for (Float m : ms) {
        totalM += m;
    }
    REQUIRE(totalM == approx(2700._f * domain.getVolume()));
}

TEST_CASE("Initial velocity", "[initial]") {
    Storage storage;
    InitialConditions conds(RunSettings::getDefaults());
    BodySettings bodySettings;
    bodySettings.set(BodySettingsId::DENSITY, 1._f);
    conds.addMonolithicBody(storage, SphericalDomain(Vector(0._f), 1._f), bodySettings)
        .addVelocity(Vector(2._f, 1._f, -1._f));
    bodySettings.set(BodySettingsId::DENSITY, 2._f);
    conds.addMonolithicBody(storage, SphericalDomain(Vector(0._f), 1._f), bodySettings)
        .addVelocity(Vector(0._f, 0._f, 1._f));
    ArrayView<Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
    ArrayView<Vector> v = storage.getAll<Vector>(QuantityId::POSITION)[1];

    auto test = [&](const Size i) -> Outcome {
        if (rho[i] == 1._f && v[i] != Vector(2._f, 1._f, -1._f)) {
            return makeFailed("Invalid velocity: ", v[i]);
        }
        if (rho[i] == 2._f && v[i] != Vector(0._f, 0._f, 1._f)) {
            return makeFailed("Invalid velocity: ", v[i]);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, v.size());
}

TEST_CASE("Initial rotation", "[initial]") {
    Storage storage;
    InitialConditions conds(RunSettings::getDefaults());
    conds.addMonolithicBody(storage, SphericalDomain(Vector(0._f), 1._f), BodySettings::getDefaults())
        .addRotation(Vector(1._f, 3._f, -2._f), BodyView::RotationOrigin::FRAME_ORIGIN);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);

    Vector axis;
    Float magnitude;
    tieToTuple(axis, magnitude) = getNormalizedWithLength(Vector(1._f, 3._f, -2._f));

    auto test = [&](const Size i) -> Outcome {
        const Float distFromAxis = getLength(r[i] - axis * dot(r[i], axis));
        if (getLength(v[i]) != approx(distFromAxis * magnitude, 1.e-6_f)) {
            return makeFailed(
                "Invalid angular velocity magnitude: \n", getLength(v[i]), " == ", distFromAxis * magnitude);
        }
        if (dot(v[i], axis) != approx(0._f, 1.e-6_f)) {
            return makeFailed("Invalid angular velocity vector: \n", v[i], " == ", axis);
        }
        if (dot(cross(r[i], v[i]), axis) <= 0) {
            return makeFailed("Invalid angular velocity direction: \n", v[i], " == ", axis);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}

TEST_CASE("Initial addHeterogeneousBody single", "[initial]") {
    BodySettings bodySettings;
    bodySettings.set(BodySettingsId::PARTICLE_COUNT, 1000);
    AutoPtr<BlockDomain> domain = makeAuto<BlockDomain>(Vector(0._f), Vector(1._f));
    Storage storage1;
    InitialConditions conds1(RunSettings::getDefaults());

    InitialConditions::BodySetup body1(std::move(domain), bodySettings);
    conds1.addHeterogeneousBody(storage1, std::move(body1), {}); // this should be equal to addBody

    Storage storage2;
    InitialConditions conds2(RunSettings::getDefaults());
    BlockDomain domain2(Vector(0._f), Vector(1._f));
    conds2.addMonolithicBody(storage2, domain2, bodySettings);
    REQUIRE(storage1.getQuantityCnt() == storage2.getQuantityCnt());
    REQUIRE(storage1.getParticleCnt() == storage2.getParticleCnt());
    REQUIRE(storage1.getMaterialCnt() == storage2.getMaterialCnt());
    iteratePair<VisitorEnum::ALL_BUFFERS>(storage1, storage2, [&](auto&& v1, auto&& v2) {
        auto test = [&](const Size i) -> Outcome {
            if (v1[i] != v2[i]) {
                return makeFailed("Different values: ", v1[i], " == ", v2[i]);
            }
            return SUCCESS;
        };
        REQUIRE(v1.size() == v2.size());
        REQUIRE_SEQUENCE(test, 0, v1.size());
    });
}

TEST_CASE("Initial addHeterogeneousBody multiple", "[initial]") {
    BodySettings bodySettings;
    bodySettings.set(BodySettingsId::PARTICLE_COUNT, 1000);
    // random to make sure we generate exactly 1000
    bodySettings.set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::RANDOM);
    Storage storage;
    InitialConditions conds(RunSettings::getDefaults());

    AutoPtr<BlockDomain> domain = makeAuto<BlockDomain>(Vector(0._f), Vector(10._f)); // [-5, 5]
    InitialConditions::BodySetup environment(std::move(domain), bodySettings);

    AutoPtr<SphericalDomain> domain1 = makeAuto<SphericalDomain>(Vector(3._f, 3._f, 2._f), 2._f);
    InitialConditions::BodySetup body1(std::move(domain1), bodySettings);

    AutoPtr<SphericalDomain> domain2 = makeAuto<SphericalDomain>(Vector(-2._f, -2._f, -1._f), 2._f);
    InitialConditions::BodySetup body2(std::move(domain2), bodySettings);

    Array<InitialConditions::BodySetup> bodies;
    bodies.emplaceBack(std::move(body1));
    bodies.emplaceBack(std::move(body2));

    BodyView view = conds.addHeterogeneousBody(storage, std::move(environment), bodies);
    const Vector vel(1._f, 2._f, 3._f);
    view.addVelocity(vel);

    REQUIRE(storage.getParticleCnt() == 1000);
    REQUIRE(storage.getMaterialCnt() == 3);
    ArrayView<Size> flag = storage.getValue<Size>(QuantityId::FLAG);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    Size particlesBody1 = 0;
    Size particlesBody2 = 0;

    // domains were moved away ...
    SphericalDomain dom1 = SphericalDomain(Vector(3._f, 3._f, 2._f), 2._f);
    SphericalDomain dom2 = SphericalDomain(Vector(-2._f, -2._f, -1._f), 2._f);
    auto test = [&](const Size i) -> Outcome {
        if (dom1.contains(r[i])) {
            particlesBody1++;
        } else if (dom2.contains(r[i])) {
            particlesBody2++;
        }
        return Outcome(flag[i] == 0 && v[i] == vel);
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
    REQUIRE(particlesBody1 >= 30);
    REQUIRE(particlesBody2 >= 30);
}

TEST_CASE("Initial addRubblePileBody", "[initial]") {
    InitialConditions ic(RunSettings::getDefaults());

    BodySettings body;
    body.set(BodySettingsId::PARTICLE_COUNT, 10000);
    body.set(BodySettingsId::MIN_PARTICLE_COUNT, 10);
    Storage storage;
    PowerLawSfd sfd;
    sfd.interval = Interval(0.2_f, 1._f);
    sfd.exponent = 3._f;
    ic.addRubblePileBody(storage, SphericalDomain(Vector(0._f), 1._f), sfd, body);

    TextOutput output(Path("rubblepile.txt"), "test", EMPTY_FLAGS);
    output.addColumn(makeAuto<ValueColumn<Vector>>(QuantityId::POSITION));
    output.addColumn(makeAuto<ValueColumn<Size>>(QuantityId::FLAG));
    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, 0._f);
    output.dump(storage, stats);
}

TEST_CASE("Initial moveToCenterOfMassSystem", "[initial]") {
    RunSettings settings;
    InitialConditions ic(settings);

    BodySettings body;
    body.set(BodySettingsId::CENTER_PARTICLES, true);
    Storage storage;
    const Vector r_com(3._f, 3._f, 2._f);
    ic.addMonolithicBody(storage, SphericalDomain(r_com, 2._f), body);


    CenterOfMass evaluator;
    REQUIRE(evaluator.evaluate(storage) == approx(r_com));

    moveToCenterOfMassSystem(
        storage.getValue<Float>(QuantityId::MASS), storage.getValue<Vector>(QuantityId::POSITION));
    REQUIRE(evaluator.evaluate(storage) == approx(Vector(0._f)));
}
