#include "Sph.h"

using namespace Sph;

// Simulations are performed by IRun object. User must derive the class and implement functions setUp, which
// sets up the simulation parameters and initial conditions af the simulation, and tearDown, which is called
// after the simulation ends.
class HelloAsteroid : public IRun {
public:
    virtual void setUp() override {
        // Create a new (empty) particle storage
        storage = makeShared<Storage>();

        // The easiest way to set up the initial conditions is via InitialConditions object.
        // Here we pass the default settings of the simulation, stored in variable 'settings'. For a list of
        // settings, see RunSettingsId enum. We also pass a scheduler object, used for parallelization.
        InitialConditions ic(*scheduler, settings);

        // Domain specifying the asteroid - sphere centered at the origin with radius 1000m.
        SphericalDomain domain(Vector(0._f), 1.e3_f);

        // Create an object holding all material parameters, initialized to defaults
        BodySettings body;

        // Set up the number of particles in the asteroid. For all material (or body-specific) parameters, see
        // BodySettingsId enum.
        body.set(BodySettingsId::PARTICLE_COUNT, 10000);

        // Add the body to the main particle storage
        ic.addMonolithicBody(*storage, domain, body);

        // Last thing - set up the duration of the simulation to 1 second
        settings.set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 1._f));
    }

    virtual void tearDown(const Statistics& stats) override {
        // Save the result of the simulation to custom binary format.
        BinaryOutput io(Path("output.ssf"));
        io.dump(*storage, stats);
    }
};

int main() {
    HelloAsteroid simulation;
    simulation.setUp();
    simulation.run();
    return 0;
}
