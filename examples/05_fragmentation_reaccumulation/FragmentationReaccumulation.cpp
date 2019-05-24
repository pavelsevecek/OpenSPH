#include "Sph.h"
#include <iostream>

using namespace Sph;

/// Custom writer of simulation log
class LogWriter : public ILogWriter {
private:
    std::string runName;

public:
    LogWriter(const RunSettings& settings)
        : ILogWriter(makeAuto<StdOutLogger>(), 1._f) {
        runName = settings.get<std::string>(RunSettingsId::RUN_NAME);
    }

    virtual void write(const Storage& UNUSED(storage), const Statistics& stats) override {
        // Timestep number and current run time
        const int index = stats.get<int>(StatisticsId::INDEX);
        const Float time = stats.get<Float>(StatisticsId::RUN_TIME);

        logger->write(runName, " #", index, "  time = ", time);
    }
};

// First phase - sets up the colliding bodies and runs the SPH simulation of the fragmentation.
class Fragmentation : public IRun {
public:
    virtual void setUp(SharedPtr<Storage> storage) override {
        settings.set(RunSettingsId::RUN_NAME, std::string("Fragmentation"));

        // Define the parameters of the target
        const Float targetRadius = 1.e5_f; // R_pb = 100km;
        BodySettings targetBody;
        targetBody
            .set(BodySettingsId::PARTICLE_COUNT, 10000) // N_pb = 10000
            .set(BodySettingsId::BODY_RADIUS, targetRadius);

        // Define the parameters of the impactor
        const Float impactorRadius = 5.e4_f;
        BodySettings impactorBody;
        impactorBody
            .set(BodySettingsId::BODY_RADIUS, impactorRadius) // R_imp = 50km
            .set(BodySettingsId::PARTICLE_COUNT, 1250);       // N_imp = 1250

        // Compute position of the impactor from the impact angle
        const Float impactAngle = 15._f * DEG_TO_RAD;
        Vector impactorPosition =
            (targetRadius + impactorRadius) * Vector(cos(impactAngle), sin(impactAngle), 0._f);
        // Move the impactor to the right, so bodies are not in contact at the start of the simulation
        impactorPosition[X] += 0.2_f * targetRadius;

        impactorBody.set(BodySettingsId::BODY_CENTER, impactorPosition);

        InitialConditions ic(settings);
        ic.addMonolithicBody(*storage, targetBody);

        // Using BodyView returned from addMonolithicBody, we can add velocity to the impactor
        BodyView impactor = ic.addMonolithicBody(*storage, impactorBody);
        impactor.addVelocity(Vector(-1.e3_f, 0._f, 0._f));

        // Run for 100s.
        settings.set(RunSettingsId::RUN_END_TIME, 100._f);

        // Save the initial (pre-impact) configuration
        BinaryOutput io(Path("start.ssf"));
        Statistics stats;
        stats.set(StatisticsId::RUN_TIME, 0._f);
        io.dump(*storage, stats);

        // Setup custom logging
        logWriter = makeAuto<LogWriter>(settings);
    }

    virtual void tearDown(const Storage& storage, const Statistics& stats) override {
        // Save the result of the fragmentation phase
        BinaryOutput io(Path("fragmented.ssf"));
        io.dump(storage, stats);
    }
};

// Second phase - uses the results from the SPH simulation as an input for N-body simulation.
class Reaccumulation : public IRun {
public:
    // Here we do not create new particles, the storage already contains particles computed by the
    // fragmentation phase. Instead we convert the SPH particles to solid spheres (hand-off).
    virtual void setUp(SharedPtr<Storage> storage) override {
        settings.set(RunSettingsId::RUN_NAME, std::string("Reaccumulation"));

        // Create an 'empty' material with default parameters. Note that we do not have to do that in other
        // examples, as it is done inside InitialConditions object.
        AutoPtr<IMaterial> material = makeAuto<NullMaterial>(BodySettings::getDefaults());

        // Create a new (empty) particle storage with our material.
        Storage spheres(std::move(material));

        // Get positions and smoothing lengths of SPH particles;
        // the smoothing lengths can be accessed using r_sph[i][H].
        Array<Vector>& r_sph = storage->getValue<Vector>(QuantityId::POSITION);

        // Get velocities of SPH particles (= derivatives of positions)
        Array<Vector>& v_sph = storage->getDt<Vector>(QuantityId::POSITION);

        // Get masses and densities of SPH particles
        Array<Float>& m_sph = storage->getValue<Float>(QuantityId::MASS);
        Array<Float>& rho_sph = storage->getValue<Float>(QuantityId::DENSITY);


        // Insert the positions of spheres into the new particle storage. We also want to initialize
        // velocities and accelerations of the sphere, hence OrderEnum::SECOND.
        // Note that Array is non-copyable, so we have to clone the data.
        spheres.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, r_sph.clone());

        // Copy also the velocities. Since the velocities were already initialized above (to zeros),
        // we can simply override the array.
        spheres.getDt<Vector>(QuantityId::POSITION) = v_sph.clone();

        // Insert the masses. Masses of spheres correspond to masses of SPH particles, so just copy them.
        // Note that masses are constant during the simulation (there are no mass derivatives), hence
        // OrderEnum::ZERO.
        spheres.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, m_sph.clone());

        // In N-body simulation, we also need the angular frequencies of particles - initialize them to zero.
        spheres.insert<Vector>(QuantityId::ANGULAR_FREQUENCY, OrderEnum::ZERO, Vector(0._f));

        // Finally, we need to set the radii of the created spheres. Let's choose the radii so that the
        // volume of the sphere is the same as the volume of the SPH particles.
        Array<Vector>& r_nbody = spheres.getValue<Vector>(QuantityId::POSITION);
        for (Size i = 0; i < r_nbody.size(); ++i) {
            r_nbody[i][H] = Sph::cbrt(3._f * m_sph[i] / (4._f * PI * rho_sph[i]));
        }

        // Once all quantities needed for N-body simulation are set up, replace the particles originally
        // passed into the function; from this point, we do not need SPH data anymore, so we can deallocate
        // them.
        *storage = std::move(spheres);


        // Here we need to select a non-default solver - instead of SPH solver, use N-body solver
        settings.set(RunSettingsId::COLLISION_HANDLER, CollisionHandlerEnum::MERGE_OR_BOUNCE)
            .set(RunSettingsId::COLLISION_OVERLAP, OverlapEnum::PASS_OR_MERGE)
            .set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::SOLID_SPHERES);

        solver = makeAuto<NBodySolver>(*scheduler, settings);

        // For manually created solvers, it is necessary to call function create for every material in the
        // storage. In this case, we only have one "sort" of particles, so we call the function just once for
        // material with index 0.
        solver->create(*storage, storage->getMaterial(0));

        // Use the leaf-frog integrator
        settings.set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::LEAP_FROG);

        // Limit the time step by accelerations. By default, the time step is also limited by CFL criterion,
        // which requires sound speed computed for every particle. Here, we do not store the sound speed, so
        // the code would report a runtime error if we used the CFL criterion.
        settings.set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::ACCELERATION)
            .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 10._f);

        // Set output quantities
        settings.set(RunSettingsId::RUN_OUTPUT_QUANTITIES,
            OutputQuantityFlag::POSITION | OutputQuantityFlag::VELOCITY | OutputQuantityFlag::MASS |
                OutputQuantityFlag::INDEX | OutputQuantityFlag::SMOOTHING_LENGTH);

        // Run for 6 hours
        settings.set(RunSettingsId::RUN_END_TIME, 60._f * 60._f * 6._f);

        // Setup custom logging
        logWriter = makeAuto<LogWriter>(settings);
    }

    virtual void tearDown(const Storage& storage, const Statistics& stats) override {
        // Save the result of the reaccumulation phase
        BinaryOutput io(Path("reaccumulated.ssf"));
        io.dump(storage, stats);
    }
};

int main() {
    try {
        Storage storage;

        // Set up and run the fragmentation phase
        Fragmentation fragmentation;
        fragmentation.run(storage);

        // Now storage contains SPH particles, we can pass them as an input to to the reaccumulation phase
        Reaccumulation reaccumulation;
        reaccumulation.run(storage);

    } catch (Exception& e) {
        std::cout << "Error during simulation: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}
