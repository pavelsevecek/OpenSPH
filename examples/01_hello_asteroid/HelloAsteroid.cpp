#include "Sph.h"
#include <iostream>

using namespace Sph;

// Simulations are performed by IRun object. User must derive the class and implement functions setUp, which
// sets up the simulation parameters and initial conditions of the simulation, and tearDown, which is called
// after the simulation ends.
class HelloAsteroid : public IRun {
public:
    virtual void setUp(SharedPtr<Storage> storage) override {

        // The easiest way to set up the initial conditions is via InitialConditions object.
        // Here we pass the default settings of the simulation, stored in variable 'settings'. For a list of
        // settings, see RunSettingsId enum.
        InitialConditions ic(settings);

        // Domain specifying the asteroid - sphere centered at the origin with radius 1000m.
        SphericalDomain domain(Vector(0._f), 1.e3_f);

        // Create an object holding all material parameters, initialized to defaults
        BodySettings body;

        // Set up the number of particles in the asteroid. For all material (or body-specific) parameters, see
        // BodySettingsId enum.
        body.set(BodySettingsId::PARTICLE_COUNT, 10000);

        // Add the body to the main particle storage
        ic.addMonolithicBody(*storage, domain, body);

        // Name the simulation
        settings.set(RunSettingsId::RUN_NAME, std::string("Hello asteroid"));

        // Last thing - set up the duration of the simulation to 1 second
        settings.set(RunSettingsId::RUN_END_TIME, 1._f);
    }

    virtual void tearDown(const Storage& storage, const Statistics& stats) override {
        // Save the result of the simulation to custom binary format.
        BinaryOutput io(Path("output.ssf"));
        io.dump(storage, stats);
    }
};

int main() {

    try {
        HelloAsteroid simulation;

        // To start the simulation, we need to provide it with a particle storage. Since our simulation
        // creates the initial conditions itself, we can simply create an empty storage here.
        Storage storage;
        simulation.run(storage);

    } const (const Exception& e) {
        // An exception may be thrown either from user-defined setUp or tearDown function, but also
        // while running the simulation, so we should always catch it.
        std::cout << "Error in simulation: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
