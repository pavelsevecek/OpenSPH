#include "gui/nbody/NBody.h"
#include "gravity/NBodySolver.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "io/FileSystem.h"
#include "io/LogFile.h"
#include "math/rng/VectorRng.h"
#include "objects/geometry/Domain.h"
#include "quantities/IMaterial.h"
#include "sph/initial/Distribution.h"
#include "sph/initial/Initial.h"
#include "system/Factory.h"
#include "system/Platform.h"
#include <wx/msgdlg.h>

IMPLEMENT_APP(Sph::App);

NAMESPACE_SPH_BEGIN

NBody::NBody() {
    settings.set(RunSettingsId::RUN_NAME, std::string("NBody"))
        .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::LEAP_FROG)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-2_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_CHANGE, 0.01_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 1.e3_f)
        .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::ACCELERATION)
        .set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 1._f)
        .set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 1.e6_f))
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 1.e4_f)
        .set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE)
        .set(RunSettingsId::GRAVITY_SOLVER, GravityEnum::BARNES_HUT)
        .set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::POINT_PARTICLES)
        .set(RunSettingsId::GRAVITY_OPENING_ANGLE, 0.5_f)
        .set(RunSettingsId::GRAVITY_LEAF_SIZE, 20)
        .set(RunSettingsId::COLLISION_HANDLER, CollisionHandlerEnum::MERGE_OR_BOUNCE)
        .set(RunSettingsId::COLLISION_OVERLAP, OverlapEnum::PASS_OR_MERGE)
        .set(RunSettingsId::COLLISION_RESTITUTION_NORMAL, 0.25_f)
        .set(RunSettingsId::COLLISION_RESTITUTION_TANGENT, 1._f)
        .set(RunSettingsId::COLLISION_ALLOWED_OVERLAP, 0.1_f)
        .set(RunSettingsId::COLLISION_MERGING_LIMIT, 1._f)
        .set(RunSettingsId::NBODY_INERTIA_TENSOR, false)
        .set(RunSettingsId::NBODY_MAX_ROTATION_ANGLE, 0.01_f)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100);
}

Float getBoundingRadius(ArrayView<Vector> r) {
    Vector center = r[0];
    Float radius = 0._f;
    for (Size i = 0; i < r.size(); ++i) {
        const Float dist = getLength(center - r[i]);
        if (dist <= radius) {
            // already inside the sphere
            continue;
        }
        radius = 0.5_f * (radius + dist);
        Vector dir = getNormalized(center - r[i]);
        center = r[i] + dir * radius;
    }
#ifdef SPH_DEBUG
    for (Size i = 0; i < r.size(); ++i) {
        ASSERT(getLength(r[i] - center) <= 1.0001_f * radius, getLength(r[i] - center), radius);
    }
#endif
    return radius;
}

Float startingRadius;

void NBody::setUp() {
    // we don't need any material, so just pass some dummy
    storage = makeShared<Storage>(makeAuto<NullMaterial>(EMPTY_SETTINGS));
    solver = makeAuto<NBodySolver>(settings);

    if (wxTheApp->argc > 1) {
        std::string arg(wxTheApp->argv[1]);
        Path path(arg);
        if (!FileSystem::pathExists(path)) {
            wxMessageBox("Cannot locate file " + path.native(), "Error", wxOK);
            return;
        }
        BinaryOutput io;
        Statistics stats;
        Outcome result = io.load(path, *storage, stats);
        if (!result) {
            wxMessageBox("Cannot load the run state file " + path.native(), "Error", wxOK);
            return;
        }
        // const Float t0 = stats.get<Float>(StatisticsId::RUN_TIME);
        // const Float dt = stats.get<Float>(StatisticsId::TIMESTEP_VALUE);
        // const Interval origRange = settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE);
        // settings.set(RunSettingsId::RUN_TIME_RANGE, Interval(t0, origRange.upper()));
        // settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, dt);

        // convert radii from SPH to nbody
        ArrayView<const Float> m = storage->getValue<Float>(QuantityId::MASS);
        ArrayView<const Float> rho = storage->getValue<Float>(QuantityId::DENSITY);
        ArrayView<Vector> r_nbody = storage->getValue<Vector>(QuantityId::POSITION);
        ASSERT(r_nbody.size() == rho.size());
        for (Size i = 0; i < r_nbody.size(); ++i) {
            r_nbody[i][H] = cbrt(3._f * m[i] / (4._f * PI * rho[i]));
        }

        // to COM
        ArrayView<Vector> r = storage->getValue<Vector>(QuantityId::POSITION);
        ArrayView<Vector> v = storage->getDt<Vector>(QuantityId::POSITION);
        Vector r_com(0._f);
        Vector v_com(0._f);
        Float totalMass = 0._f;
        for (Size i = 0; i < v.size(); ++i) {
            r_com += m[i] * r[i];
            v_com += m[i] * v[i];
            totalMass += m[i];
        }
        r_com /= totalMass;
        r_com[H] = 0._f;
        v_com /= totalMass;
        v_com[H] = 0._f;
        for (Size i = 0; i < v.size(); ++i) {
            r[i] -= r_com;
            v[i] -= v_com;
        }


    } else {
        HexagonalPacking packing(EMPTY_FLAGS);
        const Float radius = 1.e3_f;
        Array<Vector> dist = packing.generate(300000, SphericalDomain(Vector(0._f), radius));
        storage->insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(dist));
        ArrayView<Vector> r = storage->getValue<Vector>(QuantityId::POSITION);
        const Float dr = getLength(r[50] - r[51]);
        for (Size i = 0; i < r.size(); ++i) {
            r[i][H] = 0.499_f * dr;
        }

        startingRadius = 1.e3_f + r[0][H]; // getBoundingRadius(r);
        for (Size i = 0; i < r.size(); ++i) {
            ASSERT(getLength(r[i]) <= startingRadius);
        }

        storage->insert<Float>(QuantityId::MASS, OrderEnum::ZERO, 2.e5_f);
    }
    solver->create(*storage, storage->getMaterial(0));

    ASSERT(storage->isValid());

    callbacks = makeAuto<GuiCallbacks>(*controller);

    logger = Factory::getLogger(settings);
    output = makeAuto<BinaryOutput>(Path("reacc_%d.ssf"));

    triggers.pushBack(makeAuto<CommonStatsLog>(logger, settings));

    logger->write("Particles: ", storage->getParticleCnt());
}

void NBody::tearDown(const Statistics& UNUSED(stats)) {
    showNotification("NBody", "Run finished");
    ArrayView<Vector> r = storage->getValue<Vector>(QuantityId::POSITION);
    logger->write("Start radius = ", startingRadius);
    logger->write("Final radius = ", r[0][H]);
    logger->write("Shrink factor = ", int(100._f * (1._f - r[0][H] / startingRadius)), "%");
}


NAMESPACE_SPH_END
