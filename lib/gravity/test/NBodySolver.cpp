#include "gravity/NBodySolver.h"
#include "catch.hpp"
#include "gravity/Collision.h"
#include "physics/Integrals.h"
#include "quantities/IMaterial.h"
#include "tests/Approx.h"
#include "tests/Setup.h"
#include "timestepping/TimeStepping.h"
#include "utils/SequenceTest.h"
#include "utils/Utils.h"

using namespace Sph;

template <typename TFunctor>
static void integrate(SharedPtr<Storage> storage, ISolver& solver, const Float dt, TFunctor functor) {
    RunSettings settings;
    settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, dt);
    settings.set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, dt);
    settings.set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::NONE);
    EulerExplicit timestepping(storage, settings);
    Statistics stats;

    auto test = [&](Size i) -> Outcome {
        stats.set(StatisticsId::RUN_TIME, i * dt);
        timestepping.step(solver, stats);
        return functor(i);
    };
    REQUIRE_SEQUENCE(test, 1, 10000);
}

TEST_CASE("Local frame rotation", "[nbody]") {
    NBodySolver solver(RunSettings::getDefaults());
    SharedPtr<Storage> storage = makeShared<Storage>(makeAuto<NullMaterial>(EMPTY_SETTINGS));
    storage->insert<Vector>(
        QuantityId::POSITION, OrderEnum::SECOND, Array<Vector>{ Vector(0._f, 0._f, 0._f, 1._f) });
    storage->insert<Float>(QuantityId::MASS, OrderEnum::ZERO, 1._f);
    solver.create(*storage, storage->getMaterial(0));

    ArrayView<Vector> w = storage->getValue<Vector>(QuantityId::ANGULAR_VELOCITY);
    ArrayView<Vector> L = storage->getValue<Vector>(QuantityId::ANGULAR_MOMENTUM);
    ArrayView<SymmetricTensor> I = storage->getValue<SymmetricTensor>(QuantityId::MOMENT_OF_INERTIA);
    w[0] = Vector(0._f, 0._f, 2._f * PI); // 1 rotation in 1s
    L[0] = I[0] * w[0];

    ArrayView<Tensor> E = storage->getValue<Tensor>(QuantityId::LOCAL_FRAME);
    REQUIRE(E[0] == Tensor::identity());
    // the rotation takes place together with collisions
    Statistics stats;
    solver.collide(*storage, stats, 0.25_f); // quarter of a rotation
    REQUIRE(E[0] == approx(convert<Tensor>(AffineMatrix::rotateZ(PI / 2._f))));

    solver.collide(*storage, stats, 0.25_f);
    REQUIRE(E[0] == approx(convert<Tensor>(AffineMatrix::rotateZ(PI))));

    solver.collide(*storage, stats, 0.25_f);
    REQUIRE(E[0] == approx(convert<Tensor>(AffineMatrix::rotateZ(3._f / 2._f * PI))));

    solver.collide(*storage, stats, 0.25_f);
    REQUIRE(E[0] == approx(Tensor::identity()));
}

static void flywheel(const Float dt, const Float eps) {
    RunSettings settings;
    settings.set(RunSettingsId::NBODY_MAX_ROTATION_ANGLE, 1.e-4_f);
    NBodySolver solver(settings);
    SharedPtr<Storage> storage = makeShared<Storage>(makeAuto<NullMaterial>(EMPTY_SETTINGS));
    storage->insert<Vector>(QuantityId::POSITION,
        OrderEnum::SECOND,
        Array<Vector>{ Vector(0._f, 0._f, 0._f, 1._f) });            // radius 1m
    storage->insert<Float>(QuantityId::MASS, OrderEnum::ZERO, 2._f); // mass 2kg
    solver.create(*storage, storage->getMaterial(0));

    ArrayView<SymmetricTensor> I = storage->getValue<SymmetricTensor>(QuantityId::MOMENT_OF_INERTIA);
    ArrayView<Tensor> E = storage->getValue<Tensor>(QuantityId::LOCAL_FRAME);
    ArrayView<Vector> w = storage->getValue<Vector>(QuantityId::ANGULAR_VELOCITY);
    ArrayView<Vector> L = storage->getValue<Vector>(QuantityId::ANGULAR_MOMENTUM);
    w[0] = Vector(2.5_f, -4._f, 9._f);
    const Float I1 = 3._f;
    const Float I3 = 1.2_f;
    I[0] = SymmetricTensor(Vector(I1, I1, I3), Vector(0._f));
    L[0] = I[0] * w[0]; // local frame is identity matrix at the beginning, so I_loc = I_in

    const SymmetricTensor I0 = I[0];
    const Vector w0 = w[0];
    const Vector L0 = L[0];

    auto test = [&](Size) -> Outcome {
        // angular momentum must be always conserved
        const Vector L = transform(I[0], convert<AffineMatrix>(E[0])) * w[0];
        if (L != approx(L0, eps)) {
            return makeFailed("Angular momentum not conserved:\n", L, " == ", L0);
        }

        // length of the angular velocity is const
        if (getLength(w[0]) != approx(getLength(w0), eps)) {
            return makeFailed("omega not conserved:\n", getLength(w[0]), " == ", getLength(w0));
        }

        // moment of inertia should not change (must be exactly the same, not just eps-equal)
        if (I[0] != I0) {
            return makeFailed("Moment of inertia changed:\n", I[0], " == ", I0);
        }

        // angle between L and omega should be const
        if (dot(w[0], L) != approx(dot(w0, L0), eps)) {
            return makeFailed("Angle between w and L not conserved:\n", dot(w[0], L), " == ", dot(w0, L0));
        }

        return SUCCESS;
    };
    integrate(storage, solver, dt, test);

    // sanity check - omega changed
    REQUIRE(w[0] != approx(w0));
}

TEST_CASE("Flywheel small timestep", "[nbody]") {
    flywheel(1.e-5_f, 4.e-5_f);
}

TEST_CASE("Flywheel large timestep", "[nbody]") {
    flywheel(1.e-3_f, 0.01_f);
}

static SharedPtr<Storage> makeTwoParticles() {
    SharedPtr<Storage> storage = makeShared<Storage>(makeAuto<NullMaterial>(EMPTY_SETTINGS));
    storage->insert<Vector>(QuantityId::POSITION,
        OrderEnum::SECOND,
        Array<Vector>{ Vector(2._f, 0._f, 0._f, 1._f), Vector(-2._f, 0._f, 0._f, 0.5_f) });
    storage->insert<Float>(QuantityId::MASS, OrderEnum::ZERO, 2._f);

    ArrayView<Vector> v = storage->getDt<Vector>(QuantityId::POSITION);
    v[0] = Vector(-5._f, 0._f, 0._f);
    v[1] = Vector(5._f, 0._f, 0._f);
    return storage;
}

TEST_CASE("Collision bounce two", "[nbody]") {
    RunSettings settings;
    settings.set(RunSettingsId::COLLISION_HANDLER, CollisionHandlerEnum::ELASTIC_BOUNCE);
    settings.set(RunSettingsId::COLLISION_RESTITUTION_NORMAL, 1._f);
    settings.set(RunSettingsId::COLLISION_RESTITUTION_TANGENT, 1._f);
    NBodySolver solver(settings);

    SharedPtr<Storage> storage = makeTwoParticles();
    solver.create(*storage, storage->getMaterial(0));

    const Float dt = 1.e-4_f;
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
    ArrayView<const SymmetricTensor> I = storage->getValue<SymmetricTensor>(QuantityId::MOMENT_OF_INERTIA);
    ArrayView<const Vector> w = storage->getValue<Vector>(QuantityId::ANGULAR_VELOCITY);

    const Float dist = getLength(r[0] - r[1]) - r[0][H] - r[1][H];
    const Float v_rel = getLength(v[0] - v[1]);
    const Float t_coll = dist / v_rel;
    const Vector r0 = r[0];
    const Vector r1 = r[1];
    const Vector v0 = v[0];
    const Vector v1 = v[1];
    const Float I0 = I[0].trace() / 3._f;
    const Float I1 = I[1].trace() / 3._f;

    auto test = [&](Size i) -> Outcome {
        const Float t = i * dt;

        if (r[0][H] != r0[H] || r[1][H] != r1[H]) {
            return makeFailed("Radius changed");
        }
        if (I[0] != approx(SymmetricTensor::identity() * I0)) {
            return makeFailed("Moment of inertia changed\n", I[0], " == ", I0);
        }
        if (I[1] != approx(SymmetricTensor::identity() * I1)) {
            return makeFailed("Moment of inertia changed\n", I[1], " == ", I1);
        }
        if (w[0] != Vector(0._f) || w[1] != Vector(0._f)) {
            return makeFailed("Angular velocity increased");
        }
        if (storage->getParticleCnt() != 2) {
            return makeFailed("Particle number changed");
        }
        if (t < t_coll) {
            if (r[0] != approx(r0 + v0 * t) || r[1] != approx(r1 + v1 * t)) {
                return makeFailed("Incorrect positions");
            }
            if (v[0] != approx(v0, 1.e-6_f) || v[1] != approx(v1, 1.e-6_f)) {
                // clang-format off
                return makeFailed("Velocities changed before bounce\nt = ",
                    t, " (t_coll = ", t_coll, ")\n",
                    v[0], " == ", v0, "\n",
                    v[1], " == ", v1);
                // clang-format on
            }
        } else {
            if (v[0] != approx(v1, 1.e-6_f) || v[1] != approx(v0, 1.e-6_f)) {
                // clang-format off
                return makeFailed("Velocities changed after bounce\nt = ",
                    t, " (t_coll = ", t_coll, ")\n",
                    v[0], " == ", v0, "\n",
                    v[1], " == ", v1);
                // clang-format on
            }
        }
        return SUCCESS;
    };
    integrate(storage, solver, dt, test);

    // did bounce
    REQUIRE(v[0] == approx(v1, 1.e-6_f));
    REQUIRE(v[1] == approx(v0, 1.e-6_f));
}

TEST_CASE("Collision merge two", "[nbody]") {
    RunSettings settings;
    settings.set(RunSettingsId::COLLISION_HANDLER, CollisionHandlerEnum::PERFECT_MERGING);
    settings.set(RunSettingsId::COLLISION_MERGING_LIMIT, 0._f);
    NBodySolver solver(settings);

    SharedPtr<Storage> storage = makeTwoParticles();
    solver.create(*storage, storage->getMaterial(0));

    const Float dt = 1.e-4_f;
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
    ArrayView<const SymmetricTensor> I = storage->getValue<SymmetricTensor>(QuantityId::MOMENT_OF_INERTIA);
    ArrayView<const Vector> w = storage->getValue<Vector>(QuantityId::ANGULAR_VELOCITY);

    const Float dist = getLength(r[0] - r[1]) - r[0][H] - r[1][H];
    const Float v_rel = getLength(v[0] - v[1]);
    const Float t_coll = dist / v_rel;
    const Vector r0 = r[0];
    const Vector r1 = r[1];
    const Vector v0 = v[0];
    const Vector v1 = v[1];
    const Float I0 = I[0].trace() / 3._f;
    const Float I1 = I[1].trace() / 3._f;

    bool didMerge = false;
    auto test = [&](Size i) -> Outcome {
        const Float t = i * dt;

        if (t < t_coll) {
            if (storage->getParticleCnt() != 2) {
                return makeFailed("Particle number changed before merge");
            }
            if (r[0] != approx(r0 + v0 * t) || r[1] != approx(r1 + v1 * t)) {
                return makeFailed("Incorrect positions");
            }
            if (v[0] != approx(v0, 1.e-6_f) || v[1] != approx(v1, 1.e-6_f)) {
                // clang-format off
                return makeFailed("Velocities changed before merge\nt = ",
                    t, " (t_coll = ", t_coll, ")\n",
                    v[0], " == ", v0, "\n",
                    v[1], " == ", v1);
                // clang-format on
            }
            if (r[0][H] != r0[H] || r[1][H] != r1[H]) {
                return makeFailed("Radius changed");
            }
            if (I[0] != approx(SymmetricTensor::identity() * I0)) {
                return makeFailed("Moment of inertia changed\n", I[0], " == ", I0);
            }
            if (I[1] != approx(SymmetricTensor::identity() * I1)) {
                return makeFailed("Moment of inertia changed\n", I[1], " == ", I1);
            }
            if (w[0] != Vector(0._f) || w[1] != Vector(0._f)) {
                return makeFailed("Angular velocity increased");
            }
        } else {
            if (!didMerge) {
                tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
                I = storage->getValue<SymmetricTensor>(QuantityId::MOMENT_OF_INERTIA);
                w = storage->getValue<Vector>(QuantityId::ANGULAR_VELOCITY);
            }
            didMerge = true;
            if (storage->getParticleCnt() != 1) {
                return makeFailed("Particle number incorrect");
            }
            if (v[0] != approx(Vector(0._f), 1.e-6_f)) {
                // clang-format off
                return makeFailed("Incorrect velocities after merge\nt = ",
                    t, " (t_coll = ", t_coll, ")\n",
                    v[0], " == 0.\n");
                // clang-format on
            }
            if (w[0] != Vector(0._f)) {
                return makeFailed("Angular velocity increased after merge");
            }
            // I should be diagonal, smallest component xx, and yy == zz
            if (I[0].offDiagonal() != Vector(0._f)) {
                return makeFailed("Moment of inertia not diagonal after merge");
            }
            if (I[0](1, 1) != I[0](2, 2)) {
                return makeFailed("Moment of inertia not symmetric");
            }
            if (3._f * I[0](0, 0) > I[0](1, 1)) {
                return makeFailed("Loo high value of Ixx:\n", I[0](0, 0), " > ", I[0](1, 1));
            }
        }
        return SUCCESS;
    };
    integrate(storage, solver, dt, test);

    REQUIRE(didMerge);
}

TEST_CASE("Collision merge off-center", "[nbody]") {
    // hit on high impact angle should give the body some rotation
    RunSettings settings;
    settings.set(RunSettingsId::COLLISION_HANDLER, CollisionHandlerEnum::PERFECT_MERGING);
    settings.set(RunSettingsId::COLLISION_MERGING_LIMIT, 0._f);
    NBodySolver solver(settings);

    SharedPtr<Storage> storage = makeTwoParticles();
    solver.create(*storage, storage->getMaterial(0));

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
    r[0][Y] = r[0][H] + r[1][H] - 1.e-5_f;
    const Float L0 = getLength(v[0] - v[1]) * r[0][Y];
    Tensor E_prev = Tensor::null();

    bool didMerge = false;
    auto test = [&](Size) -> Outcome {
        if (storage->getParticleCnt() == 2) {
            // don't test anything till merge
            return SUCCESS;
        }
        if (storage->getParticleCnt() != 1) {
            return Outcome(false);
        }
        didMerge = true;
        ArrayView<const Vector> w = storage->getValue<Vector>(QuantityId::ANGULAR_VELOCITY);
        ArrayView<const SymmetricTensor> I =
            storage->getValue<SymmetricTensor>(QuantityId::MOMENT_OF_INERTIA);
        ArrayView<const Tensor> E = storage->getValue<Tensor>(QuantityId::LOCAL_FRAME);

        if (w[0] == approx(Vector(0._f), 0.5_f)) {
            return makeFailed("No rotation after merge:\n", w[0]);
        }
        const Float L = getLength(I[0] * w[0]);
        if (L != approx(L0, 1.e-6_f)) {
            return makeFailed("Angular momentum not conserved:\n", L, " == ", L0);
        }
        if (E[0] == approx(E_prev, 1.e-6_f)) {
            return makeFailed("Local frame not changed:\n", E[0], " == ", E_prev);
        }
        if (convert<AffineMatrix>(I[0]).isIsotropic()) {
            return makeFailed("I should not be isotropic:\n", I[0]);
        }
        E_prev = E[0];
        return SUCCESS;
    };
    integrate(storage, solver, 1.e-4_f, test);

    REQUIRE(didMerge);
}

TEST_CASE("Collision merge miss", "[nbody]") {
    RunSettings settings;
    settings.set(RunSettingsId::COLLISION_HANDLER, CollisionHandlerEnum::PERFECT_MERGING);
    settings.set(RunSettingsId::COLLISION_MERGING_LIMIT, 0._f);
    NBodySolver solver(settings);

    SharedPtr<Storage> storage = makeTwoParticles();
    solver.create(*storage, storage->getMaterial(0));

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
    r[0][Y] = r[0][H] + r[1][H] + 1.e-5_f;

    auto test = [&](Size) -> Outcome {
        if (storage->getParticleCnt() != 2) {
            return Outcome(false);
        }
        return SUCCESS;
    };
    integrate(storage, solver, 1.e-4_f, test);
}

TEST_CASE("Collision merge rejection", "[nbody]") {}

TEST_CASE("Collision repel", "[nbody]") {
    Storage storage;
    // add two overlapping particles
    storage.insert<Vector>(QuantityId::POSITION,
        OrderEnum::SECOND,
        Array<Vector>{ Vector(0._f, 0._f, 0._f, 1._f), Vector(1._f, 0._f, 0._f, 0.25_f) });
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, Array<Float>{ 1._f, 0.1_f });

    CenterOfMass com;
    const Vector com1 = com.evaluate(storage);
    RepelHandler repel(0._f, 0._f);
    repel.initialize(storage);
    FlatSet<Size> dummy;
    repel.collide(0, 1, dummy);
    const Vector com2 = com.evaluate(storage);
    REQUIRE(com1 == approx(com2));

    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    REQUIRE(r[0][H] + r[1][H] == approx(getLength(r[0] - r[1])));

    r[1] = Vector(10._f, 0._f, 0._f, 0.2_f);
    repel.initialize(storage);
    REQUIRE_ASSERT(repel.collide(0, 1, dummy));
}

TEST_CASE("Collision merge cloud", "[nbody]") {
    // just check that many particles fired into one point will all merge into a single particle
    RunSettings settings;
    settings.set(RunSettingsId::COLLISION_HANDLER, CollisionHandlerEnum::PERFECT_MERGING);
    settings.set(RunSettingsId::COLLISION_OVERLAP, OverlapEnum::FORCE_MERGE);
    settings.set(RunSettingsId::COLLISION_MERGING_LIMIT, 0._f);
    NBodySolver solver(settings);

    SharedPtr<Storage> storage = makeShared<Storage>(Tests::getStorage(100));
    solver.create(*storage, storage->getMaterial(0));

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < r.size(); ++i) {
        r[i][H] = 0.01_f;
        v[i] = -4._f * r[i];
    }
    integrate(storage, solver, 1.e-4_f, [](Size) -> Outcome { return SUCCESS; });

    // all particles should be merged into one
    REQUIRE(storage->getParticleCnt() == 1);
}

TEST_CASE("Collision merge&bounce", "[nbody]") {
    // similar to above, more or less tests that MERGE_OR_BOUNCE with REPEL overlap handler will not cause any
    // assert
    RunSettings settings;
    settings.set(RunSettingsId::COLLISION_HANDLER, CollisionHandlerEnum::MERGE_OR_BOUNCE);
    settings.set(RunSettingsId::COLLISION_OVERLAP, OverlapEnum::REPEL);
    settings.set(RunSettingsId::COLLISION_MERGING_LIMIT, 0._f);
    NBodySolver solver(settings);

    SharedPtr<Storage> storage = makeShared<Storage>(Tests::getStorage(100));
    solver.create(*storage, storage->getMaterial(0));

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < r.size(); ++i) {
        r[i][H] = 0.01_f;
        v[i] = -4._f * r[i];
    }
    integrate(storage, solver, 1.e-4_f, [](Size) -> Outcome { return SUCCESS; });

    // some particles either bounced away or were repelled in overlap, so we generally don't get 1 particle
    REQUIRE(storage->getParticleCnt() > 1);
    REQUIRE(storage->getParticleCnt() < 20);
}
