#include "Sph.h"

using namespace Sph;

// Runs a single impact simulation.
class Collision : public IRun {
public:
    virtual void setUp() override {
        storage = makeShared<Storage>();
        InitialConditions ic(*scheduler, settings);

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
        settings.set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 10._f));

        // Limit the time step by CFL criterion
        settings.set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::COURANT);

        // Set the maximal time step to 0.1s.
        settings.set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 0.1_f);
    }

    virtual void tearDown(const Statistics& stats) override {
        BinaryOutput io(Path("output.ssf"));
        io.dump(*storage, stats);
    }
};

int main() {
    Collision simulation;
    simulation.setUp();
    simulation.run();
    return 0;
}
