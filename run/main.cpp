#include "geometry/Domain.h"
#include "io/Column.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "run/Run.h"
#include "sph/initial/Initial.h"
#include "system/Profiler.h"

using namespace Sph;


/// Simple example of code usage, runs a single simulation of asteroid impact.

class Run : public Abstract::Run {
public:
    Run() {
        // Sets settings of the run; see RunSettingsId enum for all options

        settings
            // First time step; following time steps are computed from quantity derivatives and CFL criterion
            .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-6_f)

            // Maximum available time step
            .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 1.e-1_f)

            // Use force from pressure gradient in the code
            .set(RunSettingsId::MODEL_FORCE_PRESSURE_GRADIENT, true)

            // Use force from stress divergence in the code; the stress tensor is evolved using Hooke's law
            .set(RunSettingsId::MODEL_FORCE_SOLID_STRESS, true)

            // Structure for finding neighbouring particles
            .set(RunSettingsId::SPH_FINDER, FinderEnum::VOXEL)

            // Time range for the run; the run will end after 1s
            .set(RunSettingsId::RUN_TIME_RANGE, Range(0._f, 1.9e-2_f));
    }

    virtual SharedPtr<Storage> setUp() override {
        // Sets initial coditions of the run

        const std::string outputName = settings.get<std::string>(RunSettingsId::RUN_OUTPUT_NAME);
        const std::string runName = settings.get<std::string>(RunSettingsId::RUN_NAME);
        // Creates output files - save as text
        output = makeAuto<TextOutput>(outputName, runName, TextOutput::Options::SCIENTIFIC);

        // Defines columns in the output file
        output->add(makeAuto<ParticleNumberColumn>());                     // number of particles
        output->add(makeAuto<ValueColumn<Vector>>(QuantityId::POSITIONS)); // particle positions

        // Creates particle storage
        storage = makeShared<Storage>();
        InitialConditions conds(*storage, settings);

        // Set up material parameters of the bodies; see BodySettingsId enum for all options
        BodySettings body;

        body
            // Initial internal energy
            .set(BodySettingsId::ENERGY, 1.e2_f)

            // Number of particles in the body
            .set(BodySettingsId::PARTICLE_COUNT, 1'000'000)

            // Equation of state
            .set(BodySettingsId::EOS, EosEnum::TILLOTSON);


        // Create a spherical body centered at origin, radius = 500m
        SphericalDomain domain1(Vector(0._f), 500._f);
        conds.addBody(domain1, body);

        // Set up impactor parameters - same as target, only 100 SPH particles
        body.set(BodySettingsId::PARTICLE_COUNT, 100);

        // Create a spherical body, offsetted to required impact angle, radius = 20m
        SphericalDomain domain2(Vector(6.e2_f, 1.35e2_f, 0._f), 20._f);

        // Impactor with v_imp = (-5km/s, 0, 0)
        conds.addBody(domain2, body, Vector(-5.e3_f, 0._f, 0._f));

        return storage;
    }

protected:
    virtual void tearDown() override {
        // Print run statistics

        Profiler* profiler = Profiler::getInstance();
        StdOutLogger logger;
        profiler->printStatistics(logger);
    }
};


int main() {
    Run run;
    run.setUp();
    run.run();
    return 0;
}
