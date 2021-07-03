#include "Sph.h"
#include <iostream>

using namespace Sph;

// Runs a single impact simulation.
class Collision : public IRun {
public:
    virtual void setUp(SharedPtr<Storage> storage) override {
        InitialConditions ic(settings);

        BodySettings body;
        body.set(BodySettingsId::PARTICLE_COUNT, 10000);

        // Domain representing the target asteroid (R = 100km)
        SphericalDomain targetDomain(Vector(0._f), 1.e5_f);
        ic.addMonolithicBody(*storage, targetDomain, body);

        // Add only 100 particles to impatcor
        body.set(BodySettingsId::PARTICLE_COUNT, 100);

        // Domain representing the impactor (R = 20km)
        SphericalDomain impactorDomain(Vector(1.4e5_f, 0._f, 0._f), 2.e4_f);

        // Function addMonolithicBody returns an object of type BodyView, which allows to add velocity or
        // rotation to the created body.
        BodyView impactor = ic.addMonolithicBody(*storage, impactorDomain, body);

        // Set the impact velocity to 5km/s
        impactor.addVelocity(Vector(-5.e3_f, 0._f, 0._f));

        settings.set(RunSettingsId::RUN_NAME, std::string("Simple collision"));

        // Let's run the simulation for 10 seconds
        settings.set(RunSettingsId::RUN_END_TIME, 10._f);

        // Limit the time step by CFL criterion
        settings.set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::COURANT);

        // Set the maximal time step to 0.1s.
        settings.set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 0.1_f);
    }

    virtual void tearDown(const Storage& storage, const Statistics& stats) override {
        BinaryOutput io(Path("output.ssf"));
        io.dump(storage, stats);
    }
};

int main() {
    try {
        Collision simulation;
        Storage storage;
        simulation.run(storage);
    } catch (const Exception& e) {
        std::cout << "Error during simulation: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}
