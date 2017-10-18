#pragma once

#include "io/Column.h"
#include "io/FileSystem.h"
#include "io/LogFile.h"
#include "io/Output.h"
#include "run/IRun.h"
#include "run/Trigger.h"
#include "sph/equations/Potentials.h"
#include "sph/initial/Presets.h"
#include "sph/solvers/GenericSolver.h"

NAMESPACE_SPH_BEGIN

void setupCollisionColumns(TextOutput& output) {
    output.add(makeAuto<ParticleNumberColumn>());
    output.add(makeAuto<ValueColumn<Vector>>(QuantityId::POSITIONS));
    output.add(makeAuto<DerivativeColumn<Vector>>(QuantityId::POSITIONS));
    output.add(makeAuto<SmoothingLengthColumn>());
    output.add(makeAuto<ValueColumn<Float>>(QuantityId::DENSITY));
    output.add(makeAuto<ValueColumn<Float>>(QuantityId::PRESSURE));
    output.add(makeAuto<ValueColumn<Float>>(QuantityId::ENERGY));
    output.add(makeAuto<ValueColumn<Float>>(QuantityId::DAMAGE));
    output.add(makeAuto<ValueColumn<TracelessTensor>>(QuantityId::DEVIATORIC_STRESS));
}

class CollisionRun : public IRun {
private:
    Presets::CollisionParams _params;

public:
    CollisionRun(const Presets::CollisionParams params)
        : _params(params) {

        settings.set(RunSettingsId::RUN_NAME, std::string("Impact"))
            .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
            .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.01_f)
            .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 0.01_f)
            .set(RunSettingsId::RUN_TIME_RANGE, Interval(-50._f, 10._f))
            .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 0.1_f)
            .set(RunSettingsId::MODEL_FORCE_SOLID_STRESS, true)
            .set(RunSettingsId::SPH_FINDER, FinderEnum::UNIFORM_GRID)
            .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
            .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
            .set(RunSettingsId::SPH_AV_BETA, 3._f)
            .set(RunSettingsId::GRAVITY_SOLVER, GravityEnum::BARNES_HUT)
            .set(RunSettingsId::GRAVITY_OPENING_ANGLE, 0.5_f)
            .set(RunSettingsId::GRAVITY_LEAF_SIZE, 20)
            .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100)
            .set(RunSettingsId::FRAME_ANGULAR_FREQUENCY, Vector(0._f, 0._f, _params.targetRotation))
            .set(RunSettingsId::SPH_CONSERVE_ANGULAR_MOMENTUM, false)
            .set(RunSettingsId::RUN_OUTPUT_PATH, _params.outputPath.native());

        settings.saveToFile(_params.outputPath / Path("code.sph"));
    }

    virtual void setUp() override {

        BodySettings body;
        body.set(BodySettingsId::ENERGY, 0._f)
            .set(BodySettingsId::ENERGY_RANGE, Interval(0._f, INFTY))
            .set(BodySettingsId::PARTICLE_COUNT, int(_params.targetParticleCnt))
            .set(BodySettingsId::EOS, EosEnum::TILLOTSON)
            .set(BodySettingsId::STRESS_TENSOR_MIN, 1.e5_f)
            .set(BodySettingsId::RHEOLOGY_DAMAGE, DamageEnum::SCALAR_GRADY_KIPP)
            .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES)
            .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, true);

        solver = makeAuto<GenericSolver>(settings, this->getEquations(settings));
        AutoPtr<Presets::Collision> maker = makeAuto<Presets::Collision>(*solver, settings, body, _params);
        Path logPath = _params.outputPath / Path("log.txt");
        logger = makeAuto<FileLogger>(logPath, FileLogger::Options::ADD_TIMESTAMP);
        storage = makeShared<Storage>();

        maker->addTarget(*storage);
        logger->write("Created target with ", storage->getParticleCnt(), " particles");


        this->setupOutput();
        this->setupTriggers(std::move(maker));

        // add printing of run progres
        triggers.pushBack(makeAuto<CommonStatsLog>(logger));
    }

private:
    virtual void tearDown() override {
        PkdgravParams params;
        params.omega = settings.get<Vector>(RunSettingsId::FRAME_ANGULAR_FREQUENCY);
        PkdgravOutput pkdgravOutput(_params.outputPath / Path("pkdgrav/pkdgrav.out"), std::move(params));
        Statistics stats;
        pkdgravOutput.dump(*storage, stats);
    }

    void setupOutput() {
        Path outputPath = Path(settings.get<std::string>(RunSettingsId::RUN_OUTPUT_PATH)) /
                          Path(settings.get<std::string>(RunSettingsId::RUN_OUTPUT_NAME));
        const std::string name = settings.get<std::string>(RunSettingsId::RUN_NAME);
        AutoPtr<TextOutput> textOutput =
            makeAuto<TextOutput>(outputPath, name, TextOutput::Options::SCIENTIFIC);
        setupCollisionColumns(*textOutput);
        output = std::move(textOutput);
    }


    void setupTriggers(AutoPtr<Presets::Collision>&& maker) {

        // Trigger creating impactor at t = 0
        class ImpactorTrigger : public ITrigger {
        private:
            AutoPtr<Presets::Collision> maker;
            Float startTime;

        public:
            explicit ImpactorTrigger(AutoPtr<Presets::Collision>&& maker, const Float startTime)
                : maker(std::move(maker))
                , startTime(startTime) {}

            virtual TriggerEnum type() const override {
                return TriggerEnum::ONE_TIME;
            }

            virtual bool condition(const Storage& UNUSED(storage), const Statistics& stats) override {
                const Float t = stats.get<Float>(StatisticsId::RUN_TIME);
                return t >= startTime;
            }

            virtual AutoPtr<ITrigger> action(Storage& storage, Statistics& UNUSED(stats)) override {
                maker->addImpactor(storage);
                /* const Size targetParticleCnt = storage.getParticleCnt();
                 /// \todo add logger here, either global or as a member variable
                 * globalLogger.write(
                    "Added impactor, particle cnt = ", storage.getParticleCnt() - targetParticleCnt);*/
                return nullptr;
            }
        };

        class DumpTrigger : public ITrigger {
        private:
            /// Time range of the simulation.
            Interval timeRange;

            /// Time when initial dampening phase is ended and impact starts
            Float startTime = 0._f;

            /// Velocity damping constant
            Float delta = 0.5_f;

        public:
            explicit DumpTrigger(const RunSettings& settings) {
                timeRange = settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE);
            }

            virtual TriggerEnum type() const override {
                return TriggerEnum::REPEATING;
            }

            virtual bool condition(const Storage& UNUSED(storage), const Statistics& stats) override {
                const Float t = stats.get<Float>(StatisticsId::RUN_TIME);

                // only dump before the start of the simulation
                return t < startTime;
            }

            virtual AutoPtr<ITrigger> action(Storage& storage, Statistics& stats) override {
                const Float t = stats.get<Float>(StatisticsId::RUN_TIME);
                const Float dt = stats.getOr<Float>(StatisticsId::TIMESTEP_VALUE, 0.01_f);
                ASSERT(t < startTime);
                // damp velocities
                ArrayView<Vector> r, v, dv;
                tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
                for (Size i = 0; i < v.size(); ++i) {
                    // gradually decrease the delta
                    const Float t0 = timeRange.lower();
                    const Float factor =
                        1._f + lerp(delta * dt, 0._f, min((t - t0) / (startTime - t0), 1._f));
                    // we have to dump only the deviation of velocities, not the initial rotation!
                    v[i] /= factor;
                    ASSERT(isReal(v[i]));
                }

                if (storage.has(QuantityId::DAMAGE)) {
                    // zero all damage potentially created during the initial phase
                    ArrayView<Float> d = storage.getValue<Float>(QuantityId::DAMAGE);
                    for (Size i = 0; i < d.size(); ++i) {
                        d[i] = 0._f;
                    }
                }
                return nullptr;
            }
        };
        triggers.pushBack(makeAuto<ImpactorTrigger>(std::move(maker), 0._f));
        triggers.pushBack(makeAuto<DumpTrigger>(settings));
    }

    EquationHolder getEquations(const RunSettings& settings) const {
        // here we cannot use member variables as they haven't been initialized yet

        EquationHolder equations;

        // forces
        equations += makeTerm<PressureForce>(settings) + makeTerm<SolidStressForce>(settings);

        // noninertial acceleration
        const Vector omega = settings.get<Vector>(RunSettingsId::FRAME_ANGULAR_FREQUENCY);
        equations += makeTerm<InertialForce>(omega);

        // gravity (approximation)
        equations += makeTerm<SphericalGravity>(SphericalGravity::Options::ASSUME_HOMOGENEOUS);

        // density evolution
        equations += makeTerm<ContinuityEquation>(settings);

        // artificial viscosity
        equations += EquationHolder(Factory::getArtificialViscosity(settings));

        // adaptive smoothing length
        equations += makeTerm<AdaptiveSmoothingLength>(settings);

        return equations;
    }
};

NAMESPACE_SPH_END
