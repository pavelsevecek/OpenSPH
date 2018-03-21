#include "gui/reacc/Reacc.h"
#include "gravity/NBodySolver.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "io/FileSystem.h"
#include "io/LogFile.h"
#include "math/rng/VectorRng.h"
#include "objects/geometry/Domain.h"
#include "quantities/IMaterial.h"
#include "quantities/Iterate.h"
#include "sph/initial/Distribution.h"
#include "sph/initial/Initial.h"
#include "sph/initial/Presets.h"
#include "sph/solvers/StabilizationSolver.h"
#include "system/Factory.h"
#include "system/Platform.h"
#include <fstream>
#include <wx/msgdlg.h>

IMPLEMENT_APP(Sph::App);

NAMESPACE_SPH_BEGIN

template <typename T>
static Outcome checkBuffer(const Array<T>& buffer) {
    for (Size i = 0; i < buffer.size(); ++i) {
        if (!isReal(buffer[i])) {
            return makeFailed("Inf or NaN detected for particle ", i, ": ", buffer[i]);
        }
    }
    return SUCCESS;
}

template <typename T>
Outcome checkValues(const Array<T>& buffer, const Storage& UNUSED(storage), const QuantityId UNUSED(id)) {
    return checkBuffer(buffer);
}

template <>
Outcome checkValues(const Array<Float>& buffer, const Storage& storage, const QuantityId id) {
    ArrayView<const Size> matIds = storage.getValue<Size>(QuantityId::MATERIAL_ID);
    for (Size i = 0; i < buffer.size(); ++i) {
        if (!isReal(buffer[i])) {
            return makeFailed("Inf or NaN detected for particle ", i, ": ", buffer[i]);
        }
        MaterialView mat = storage.getMaterial(matIds[i]);
        const Interval range = mat->range(id);
        if (buffer[i] < range.lower() || buffer[i] > range.upper()) {
            return makeFailed("Quantity value outside of allowed range: ",
                buffer[i],
                " not inside [",
                range.lower(),
                ",",
                range.upper(),
                "]");
        }
    }
    return SUCCESS;
}

static Outcome checkStorage(const Storage& storage) {
    std::string final;
    auto mergeError = [&final](const Outcome& result, QuantityId id, std::string type) {
        if (!result) {
            final += "Problem detected in " + type + " of " + getMetadata(id).quantityName + "\n" +
                     result.error() + "\n\n";
        }
    };
    iterate<VisitorEnum::ZERO_ORDER>(storage, [&storage, &mergeError](QuantityId id, const auto& buffer) {
        mergeError(checkValues(buffer, storage, id), id, "values");

    });
    iterate<VisitorEnum::FIRST_ORDER>(
        storage, [&storage, &mergeError](QuantityId id, const auto& buffer, const auto& dv) {
            mergeError(checkValues(buffer, storage, id), id, "values");
            mergeError(checkBuffer(dv), id, "derivatives");
        });
    iterate<VisitorEnum::SECOND_ORDER>(
        storage, [&storage, &mergeError](QuantityId id, const auto& buffer, const auto& dv, const auto& d2v) {
            mergeError(checkValues(buffer, storage, id), id, "values");
            mergeError(checkBuffer(dv), id, "derivatives");
            mergeError(checkBuffer(d2v), id, "2nd derivatives");
        });

    if (final.empty()) {
        return SUCCESS;
    } else {
        return Outcome(final);
    }
}

// Trigger dumping the run state when the timestep drop to very low values, suggesting a problem in the run.
class ErrorDump : public ITrigger {
public:
    virtual TriggerEnum type() const override {
        return TriggerEnum::ONE_TIME;
    }

    virtual bool condition(const Storage& UNUSED(storage), const Statistics& stats) override {
        const Float dt = stats.get<Float>(StatisticsId::TIMESTEP_VALUE);
        return dt < 1.e-9_f;
    }

    virtual AutoPtr<ITrigger> action(Storage& storage, Statistics& stats) override {
        BinaryOutput io(Path("errordump.ssf"));
        io.dump(storage, stats);
        StdOutLogger().write("TIMESTEP DROP DETECTED: run state saved as 'errordump.ssf'");
        Outcome result = checkStorage(storage);
        if (!result) {
            std::ofstream ofs("storageerror.txt");
            ofs << result.error();
            StdOutLogger().write("ERROR DETECTED IN STORAGE: log saved as 'storageerror.txt'");
        }
        return nullptr;
    }
};


/// Shared settings for stabilization and fragmentation
RunSettings getSharedSettings() {
    RunSettings settings;
    settings.set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.01_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 100._f)
        .set(RunSettingsId::TIMESTEPPING_COURANT_NEIGHBOUR_LIMIT, 10)
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 100._f)
        .setFlags(RunSettingsId::SOLVER_FORCES,
            ForceEnum::PRESSURE_GRADIENT | ForceEnum::SOLID_STRESS |
                ForceEnum::GRAVITY) //| ForceEnum::INERTIAL)
        .set(RunSettingsId::SOLVER_TYPE, SolverEnum::ASYMMETRIC_SOLVER)
        .set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE)
        .set(RunSettingsId::SPH_FORMULATION, FormulationEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
        .set(RunSettingsId::SPH_AV_BETA, 3._f)
        .set(RunSettingsId::SPH_KERNEL_ETA, 1.3_f)
        .set(RunSettingsId::GRAVITY_SOLVER, GravityEnum::BARNES_HUT)
        .set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::SPH_KERNEL)
        .set(RunSettingsId::GRAVITY_OPENING_ANGLE, 0.8_f)
        .set(RunSettingsId::GRAVITY_LEAF_SIZE, 20)
        //.set(RunSettingsId::TIMESTEPPING_MEAN_POWER, -0._f)
        .set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 0.2_f)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100)
        .set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST)
        .set(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR, true)
        .set(RunSettingsId::SPH_STABILIZATION_DAMPING, 0.1_f)
        .set(RunSettingsId::FRAME_ANGULAR_FREQUENCY, Vector(0._f));
    // Vector(0._f, 0._f, 2._f * PI / (6._f * 3600._f)));
    return settings;
}

Stabilization::Stabilization(RawPtr<Controller> newController) {
    settings = getSharedSettings();
    settings.set(RunSettingsId::RUN_NAME, std::string("Stabilization"));

    if (wxTheApp->argc > 1) {
        // continue run, we don't need to do the stabilization, so skip it by settings the range to zero
        settings.set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 0._f));
    } else {
        settings.set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 200._f));
    }
    controller = newController;
}

Stabilization::~Stabilization() {}

void Stabilization::setUp() {
    solver = makeAuto<StabilizationSolver>(settings);
    storage = makeShared<Storage>();

    if (wxTheApp->argc > 1) {
        std::string arg(wxTheApp->argv[1]);
        Path path(arg);
        if (!FileSystem::pathExists(path)) {
            wxMessageBox("Cannot locate file " + path.native(), "Error", wxOK);
            return;
        } else {
            BinaryOutput io;
            Statistics stats;
            Outcome result = io.load(path, *storage, stats);
            if (!result) {
                wxMessageBox("Cannot load the run state file " + path.native(), "Error", wxOK);
                return;
            } else {
                // const Float t0 = stats.get<Float>(StatisticsId::RUN_TIME);
                const Float dt = stats.get<Float>(StatisticsId::TIMESTEP_VALUE);
                // const Interval origRange = settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE);
                // settings.set(RunSettingsId::RUN_TIME_RANGE, Interval(t0, origRange.upper()));
                settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, dt);
            }
        }
    } else {

        Size N = 50'000;

        BodySettings body;
        body.set(BodySettingsId::ENERGY, 0._f)
            .set(BodySettingsId::ENERGY_RANGE, Interval(0._f, INFTY))
            .set(BodySettingsId::EOS, EosEnum::TILLOTSON)
            .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::SCALAR_GRADY_KIPP)
            .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES)
            .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, true)
            .set(BodySettingsId::SHEAR_VISCOSITY, 1.e12_f)
            .set(BodySettingsId::BULK_VISCOSITY, 0._f)
            .set(BodySettingsId::ENERGY_MIN, 10._f)
            .set(BodySettingsId::DAMAGE_MIN, 0.5_f);
        //.set(BodySettingsId::DIELH_STRENGTH, 0.1_f);

        Presets::CollisionParams params;
        params.targetRadius = 50.e3_f;     // D = 100km
        params.projectileRadius = 0.5e3_f; // D = 1km
        params.impactAngle = 75._f * DEG_TO_RAD;
        params.impactSpeed = 5.e3_f;
        params.targetRotation = 2._f * PI / (3._f * 3600._f);
        params.targetParticleCnt = N;
        params.impactorParticleCntOverride = 10;

        /*const Vector impactPoint(params.targetRadius * cos(params.impactAngle),
            params.targetRadius * sin(params.impactAngle),
            0._f);
        const Float homogeneousRadius = 0.25_f * params.targetRadius;
        params.concentration = [impactPoint, homogeneousRadius](const Vector& v) {
            const Float distSqr = getSqrLength(v - impactPoint);
            if (distSqr < sqr(homogeneousRadius)) {
                return 1._f;
            } else {
                return sqr(sqr(homogeneousRadius) / distSqr);
            }
        };*/

        data = makeShared<Presets::Collision>(*solver, settings, body, params);
        data->addTarget(*storage);
    }

    callbacks = makeAuto<GuiCallbacks>(*controller);
    triggers.pushBack(makeAuto<CommonStatsLog>(Factory::getLogger(settings)));
    output = makeAuto<BinaryOutput>(Path("stab_%d.ssf"));
}

AutoPtr<IRunPhase> Stabilization::getNextPhase() const {
    return makeAuto<Fragmentation>(data, onSphFinished);
}

void Stabilization::tearDown() {
    if (wxTheApp->argc == 1) {
        data->addImpactor(*storage);
    }
}


Fragmentation::Fragmentation(SharedPtr<Presets::Collision> data, Function<void()> onFinished)
    : data(data)
    , onFinished(onFinished) {
    settings = getSharedSettings();
    settings.set(RunSettingsId::RUN_NAME, std::string("Fragmentation"))
        .set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 10'000._f))
        //.set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 0.8_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 1000._f);
}

Fragmentation::~Fragmentation() = default;

void Fragmentation::setUp() {
    /*  storage = makeShared<Storage>();
      solver = Factory::getSolver(settings);

      if (wxTheApp->argc > 1) {
          std::string arg(wxTheApp->argv[1]);
          Path path(arg);
          if (!FileSystem::pathExists(path)) {
              wxMessageBox("Cannot locate file " + path.native(), "Error", wxOK);
              return;
          } else {
              BinaryOutput io;
              Statistics stats;
              Outcome result = io.load(path, *storage, stats);
              if (!result) {
                  wxMessageBox("Cannot load the run state file " + path.native(), "Error", wxOK);
                  return;
              } else {
                  // const Float t0 = stats.get<Float>(StatisticsId::RUN_TIME);
                  const Float dt = stats.get<Float>(StatisticsId::TIMESTEP_VALUE);
                  // const Interval origRange = settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE);
                  // settings.set(RunSettingsId::RUN_TIME_RANGE, Interval(t0, origRange.upper()));
                  settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, dt);
              }
          }

      } else {
          InitialConditions ic(*solver, settings);

          Float v = 230._f;
          Float r = 3e6;
          Vector pos(-3._f * r, 0.8_f * r, 0._f);
          SphericalDomain domain1(pos, r); // D = 10km
          ic.addMonolithicBody(*storage, domain1, body).addVelocity(Vector(v, 0._f, 0._f));

          ArrayView<Vector> d = storage->getValue<Vector>(QuantityId::POSITION);
          ArrayView<Float> u = storage->getValue<Float>(QuantityId::ENERGY);
          Analytic::StaticSphere sphere(r, 2700._f);
          TillotsonEos eos(body);
          for (Size i = 0; i < d.size(); ++i) {
              const Float p = sphere.getPressure(getLength(d[i] - pos));
              u[i] = eos.getInternalEnergy(2700._f, p);
          }

          body.set(BodySettingsId::PARTICLE_COUNT, int(N / 8))
              .set(BodySettingsId::STRESS_TENSOR_MIN, LARGE)
              .set(BodySettingsId::DAMAGE_MIN, LARGE);
          // SphericalDomain domain2(Vector(5097.4509902022_f, 3726.8662269290_f, 0._f), 270.5847632732_f);
          // SphericalDomain domain2(Vector(7000._f, 1000._f, 0._f), 400._f);
          SphericalDomain domain2(Vector(3._f * r, -0.8f * r, 0._f), 0.5_f * r); // D = 10km
          ic.addMonolithicBody(*storage, domain2, body).addVelocity(Vector(-8._f * v, 0._f, 0._f));
      }

      callbacks = makeAuto<GuiCallbacks>(*controller);

      output = makeAuto<BinaryOutput>(Path("out%d.ssf"));*/

    // add printing of run progres
    triggers.pushBack(makeAuto<CommonStatsLog>(Factory::getLogger(settings)));
    triggers.pushBack(makeAuto<ErrorDump>());
}

void Fragmentation::handoff(Storage&& input) {
    storage = makeShared<Storage>(std::move(input));
    solver = Factory::getSolver(settings);
    // the quantities are already created, no need to call solver->create
    triggers.pushBack(makeAuto<CommonStatsLog>(Factory::getLogger(settings)));
    output = makeAuto<BinaryOutput>(Path("frag_%d.ssf"));

    /*    ArrayView<const Vector> r = storage->getValue<Vector>(QuantityId::POSITION);
        ArrayView<Float> dmg = storage->getValue<Float>(QuantityId::DAMAGE);
        for (Size i = 0; i < r.size(); ++i) {
            if (getLength(r[i] - data->getImpactPoint()) < 5.e3_f) {
                dmg[i] = 1._f;
            }
        }*/

    /*for (Size matId = 0; matId < storage->getMaterialCnt(); ++matId) {
        storage->getMaterial(matId)->setRange(QuantityId::DEVIATORIC_STRESS, Interval::unbounded(), 1.e7_f);
    }*/
}

AutoPtr<IRunPhase> Fragmentation::getNextPhase() const {
    return nullptr; // return makeAuto<Reaccumulation>();
}

void Fragmentation::tearDown() {
    onFinished();
}

Reaccumulation::Reaccumulation() {
    settings.set(RunSettingsId::RUN_NAME, std::string("Reacc"))
        .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::LEAP_FROG)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.1_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 500._f)
        .set(RunSettingsId::TIMESTEPPING_MAX_CHANGE, 0.05_f)
        .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::ACCELERATION)
        .set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 0.5_f)
        .set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 1._f)) // 30.f * 24._f * 3600._f))
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 1.e10_f)
        .set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE)
        .set(RunSettingsId::GRAVITY_SOLVER, GravityEnum::BARNES_HUT)
        .set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::SOLID_SPHERES)
        .set(RunSettingsId::GRAVITY_OPENING_ANGLE, 0.8_f)
        .set(RunSettingsId::GRAVITY_LEAF_SIZE, 20)
        .set(RunSettingsId::COLLISION_HANDLER, CollisionHandlerEnum::ELASTIC_BOUNCE)
        .set(RunSettingsId::COLLISION_OVERLAP, OverlapEnum::REPEL)
        .set(RunSettingsId::COLLISION_RESTITUTION_NORMAL, 0.5_f)
        .set(RunSettingsId::COLLISION_RESTITUTION_TANGENT, 1._f)
        .set(RunSettingsId::COLLISION_ALLOWED_OVERLAP, 0.1_f)
        .set(RunSettingsId::COLLISION_MERGING_LIMIT, 1._f)
        .set(RunSettingsId::NBODY_INERTIA_TENSOR, false)
        .set(RunSettingsId::NBODY_MAX_ROTATION_ANGLE, 0.01_f)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100);
}

void Reaccumulation::setUp() {
    STOP;
}

void Reaccumulation::handoff(Storage&& sph) {
    // we don't need any material, so just pass some dummy
    solver = makeAuto<NBodySolver>(settings);

    storage = makeShared<Storage>(makeAuto<NullMaterial>(EMPTY_SETTINGS));
    storage->insert<Vector>(
        QuantityId::POSITION, OrderEnum::SECOND, sph.getValue<Vector>(QuantityId::POSITION).clone());
    storage->getDt<Vector>(QuantityId::POSITION) = sph.getDt<Vector>(QuantityId::POSITION).clone();
    storage->insert<Float>(QuantityId::MASS, OrderEnum::ZERO, sph.getValue<Float>(QuantityId::MASS).clone());

    // radii handoff
    ArrayView<const Float> m = sph.getValue<Float>(QuantityId::MASS);
    ArrayView<const Float> rho = sph.getValue<Float>(QuantityId::DENSITY);
    ArrayView<Vector> r_nbody = storage->getValue<Vector>(QuantityId::POSITION);
    ASSERT(r_nbody.size() == rho.size());
    for (Size i = 0; i < r_nbody.size(); ++i) {
        r_nbody[i][H] = 0.75_f * cbrt(3._f * m[i] / (4._f * PI * rho[i]));
    }

    Array<Size> toRemove;
    ArrayView<const Float> u = sph.getValue<Float>(QuantityId::ENERGY);
    for (Size matId = 0; matId < sph.getMaterialCnt(); ++matId) {
        MaterialView view = sph.getMaterial(matId);
        const Float u_max = view->getParam<Float>(BodySettingsId::TILLOTSON_SUBLIMATION);
        for (Size i : view.sequence()) {
            if (u[i] > u_max) {
                toRemove.push(i);
            }
        }
    }
    storage->remove(toRemove);

    // to COM system
    ArrayView<Vector> v = storage->getDt<Vector>(QuantityId::POSITION);
    ArrayView<Float> m2 = storage->getValue<Float>(QuantityId::MASS);
    Vector v_com(0._f);
    Float totalMass = 0._f;
    for (Size i = 0; i < v.size(); ++i) {
        v_com += m2[i] * v[i];
        totalMass += m2[i];
    }
    v_com /= totalMass;
    for (Size i = 0; i < v.size(); ++i) {
        v[i] -= v_com;
    }

    solver->create(*storage, storage->getMaterial(0));
    ASSERT(storage->isValid());
    triggers.pushBack(makeAuto<CommonStatsLog>(Factory::getLogger(settings)));

    //    onRunStarted(*storage);
}

void Reaccumulation::tearDown() {
    showNotification("Reacc", "Run finished");
}


NAMESPACE_SPH_END
