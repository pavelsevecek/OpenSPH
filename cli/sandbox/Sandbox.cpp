#include "Sph.h"
#include <fstream>

using namespace Sph;

template <typename TTimeStepping>
static void plotTimeStepping(const std::string name) {
    SharedPtr<Storage> storage = makeShared<Storage>(makeAuto<NullMaterial>(BodySettings::getDefaults()));
    Quantity& particles = storage->insert<Vector>(QuantityId::POSITION,
        OrderEnum::SECOND,
        {
            Vector(0._f, 0._f, 0._f, 1.e6_f),
            Vector(Constants::au, 0._f, 0._f, 1.e6_f),
        });
    particles.getDt<Vector>() = Array<Vector>{
        Vector(0._f),
        Vector(0._f, sqrt(Constants::gravity * Constants::M_sun / Constants::au), 0._f),
    };
    storage->insert<Float>(QuantityId::MASS, OrderEnum::ZERO, { Constants::M_sun, 0.75f * Constants::M_sun });
    storage->insert<Vector>(QuantityId::ANGULAR_FREQUENCY, OrderEnum::ZERO, Vector(0._f));

    RunSettings settings;
    settings.set(RunSettingsId::TIMESTEPPING_CRITERION, EMPTY_FLAGS)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e5_f)
        .set(RunSettingsId::GRAVITY_SOLVER, GravityEnum::BRUTE_FORCE);

    SharedPtr<IScheduler> scheduler = Factory::getScheduler(settings);
    HardSphereSolver solver(*scheduler, settings);

    TTimeStepping timestepping(storage, settings);
    Statistics stats;

    std::ofstream ofs(name);

    ArrayView<Float> m = storage->getValue<Float>(QuantityId::MASS);
    ArrayView<Vector> r = storage->getValue<Vector>(QuantityId::POSITION);
    ArrayView<Vector> v = storage->getDt<Vector>(QuantityId::POSITION);
    moveToCenterOfMassSystem(m, r);
    moveToCenterOfMassSystem(m, v);

    Float t = 0;
    while (t < 5000 * 24 * 3600) {
        timestepping.step(*scheduler, solver, stats);
        t += timestepping.getTimeStep();

        ofs << r[0][X] << "  " << r[0][Y] << "  " << r[1][X] << "  " << r[1][Y] << std::endl;
    }
}

int main(int, char**) {
    plotTimeStepping<EulerExplicit>("euler.txt");
    plotTimeStepping<PredictorCorrector>("pc.txt");
    plotTimeStepping<LeapFrog>("leapfrog.txt");
    plotTimeStepping<ModifiedMidpointMethod>("mm.txt");
    plotTimeStepping<RungeKutta>("rk.txt");
    return 0;
}
