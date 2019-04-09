#include "Sph.h"

using namespace Sph;

// First phase - sets up the colliding bodies and runs the SPH simulation of the fragmentation.
class Fragmentation : public IRun {
public:
    virtual void setUp() override {
        settings.set(RunSettingsId::RUN_NAME, std::string("Fragmentation"));

        storage = makeShared<Storage>();

        // Here we use the CollisionParams for simple collision setup
        CollisionParams params;

        params.geometry
            .set(CollisionGeometrySettingsId::TARGET_RADIUS, 1.e5_f)        // R_pb = 100km
            .set(CollisionGeometrySettingsId::TARGET_PARTICLE_COUNT, 10000) // N_pb = 10000
            .set(CollisionGeometrySettingsId::IMPACTOR_RADIUS, 5.e4_f)      // R_imp = 50km
            .set(CollisionGeometrySettingsId::IMPACT_ANGLE, 15._f)          // phi_imp = 15deg
            .set(CollisionGeometrySettingsId::IMPACT_SPEED, 1.e3_f);        // v_imp = 1km/s

        CollisionInitialConditions ic(*scheduler, settings, params);
        ic.addTarget(*storage);
        ic.addImpactor(*storage);

        // Run for 500s.
        settings.set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 500._f));

        // Save the initial (pre-impact) configuration
        BinaryOutput io(Path("start.ssf"));
        Statistics stats;
        stats.set(StatisticsId::RUN_TIME, 0._f);
        io.dump(*storage, stats);
    }

    virtual void tearDown(const Statistics& stats) override {
        // Save the result of the fragmentation phase
        BinaryOutput io(Path("fragmented.ssf"));
        io.dump(*storage, stats);
    }
};

// Second phase - uses the results from the SPH simulation as an input for N-body simulation.
class Reaccumulation : public IRun {
private:
    // Holds the SPH particles
    SharedPtr<Storage> fragmentationResult;

public:
    Reaccumulation(SharedPtr<Storage> fragmentationResult)
        : fragmentationResult(fragmentationResult) {}

    // Here we need to fill the particle storage, using the output of the fragmentation phase (hand-off).
    virtual void setUp() override {
        settings.set(RunSettingsId::RUN_NAME, std::string("Reaccumulation"));

        // Create an 'empty' material with default parameters. Note that we do not have to do that in other
        // examples, as it is done inside InitialConditions object.
        AutoPtr<IMaterial> material = makeAuto<NullMaterial>(BodySettings::getDefaults());

        // Create a particle storage with our material.
        storage = makeShared<Storage>(std::move(material));

        // Get positions and smoothing lengths of SPH particles;
        // the smoothing lengths can be accessed using r_sph[i][H].
        Array<Vector>& r_sph = fragmentationResult->getValue<Vector>(QuantityId::POSITION);

        // Get velocities of SPH particles (= derivatives of positions)
        Array<Vector>& v_sph = fragmentationResult->getDt<Vector>(QuantityId::POSITION);

        // Get masses and densities of SPH particles
        Array<Float>& m_sph = fragmentationResult->getValue<Float>(QuantityId::MASS);
        Array<Float>& rho_sph = fragmentationResult->getValue<Float>(QuantityId::DENSITY);


        // Insert the positions of spheres into the new particle storage. We also want to initialize
        // velocities and accelerations of the sphere, hence OrderEnum::SECOND.
        // Note that Array is non-copyable, so we have to clone the data.
        storage->insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, r_sph.clone());

        // Copy also the velocities. Since the velocities were already initialized above (to zeroes),
        // we can simply override the array.
        storage->getDt<Vector>(QuantityId::POSITION) = v_sph.clone();

        // Insert the masses. Masses of spheres correspond to masses of SPH particles, so just copy them.
        // Note that masses are constant during the simulation (there are not mass derivatives), hence
        // OrderEnum::ZERO.
        storage->insert<Float>(QuantityId::MASS, OrderEnum::ZERO, m_sph.clone());

        // In N-body simulation, we also need the angular frequencies of particles - initialize them to zero.
        storage->insert<Vector>(QuantityId::ANGULAR_FREQUENCY, OrderEnum::ZERO, Vector(0._f));

        // Finally, we need to set the radii of the created spheres. Let's choose the radii so that the
        // volume of the sphere is the same as the volume of the SPH particles.
        Array<Vector>& r_nbody = storage->getValue<Vector>(QuantityId::POSITION);
        for (Size i = 0; i < r_nbody.size(); ++i) {
            r_nbody[i][H] = Sph::cbrt(3._f * m_sph[i] / (4._f * PI * rho_sph[i]));
        }

        // Here we need to select a non-default solver - instead of SPH solver, use N-body solver
        settings.set(RunSettingsId::COLLISION_HANDLER, CollisionHandlerEnum::MERGE_OR_BOUNCE);
        settings.set(RunSettingsId::COLLISION_OVERLAP, OverlapEnum::PASS_OR_MERGE);
        settings.set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::SOLID_SPHERES);
        solver = makeAuto<NBodySolver>(*scheduler, settings);

        // Use the leaf-frog integrator
        settings.set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::LEAP_FROG);

        // Limit the time step by accelerations. Note that by default, the time step is also limited by CFL
        // criterion, which requires sound speed computed for every particle. Here, we do not store the sound
        // speed, so the code would report a runtime error if we used the CFL criterion.
        settings.set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::ACCELERATION);
        settings.set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 10._f);

        // Run for 2 days
        settings.set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 2._f * 60._f * 60._f * 24._f));
    }

    virtual void tearDown(const Statistics& stats) override {
        // Save the result of the reaccumulation phase
        BinaryOutput io(Path("reaccumulated.ssf"));
        io.dump(*storage, stats);
    }
};

int main() {
    // Set up and run the fragmentation phase
    Fragmentation fragmentation;
    fragmentation.setUp();
    fragmentation.run();

    // We can access the results of the fragmentation phase using getStorage function.
    SharedPtr<Storage> fragmentationResult = fragmentation.getStorage();

    // Pass the results of fragmentation phase to the input of the reaccumulation phase
    Reaccumulation reaccumulation(fragmentationResult);

    // Run the reaccumulation
    reaccumulation.setUp();
    reaccumulation.run();
    return 0;
}
