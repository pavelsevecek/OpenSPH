/// \brief Executable running a single impact simulation, using command-line parameters
///
/// This program expects the following parameters:
///  - radius of the target asteroid (in meters)
///  - rotational period of the target (in hours)
///  - impact energy Q/Q*_D; 0.01 means cratering event, 100 catastrophic event, etc.
///  - speed of the impactor (in km/s)
///  - impact angle in degrees
///
/// The output is stored in directory named using the parameters, for example:
/// sph_5000m_285m_3h_5kms_315 (the numbers correspond to the target radius, impactor radius, period of the
/// target, impact velocity, and impact angle).
///
/// When executed for the first time, the simulation is started using default parameters and the configuration
/// files (with extension .sph) are saved to the created directory. These files can be then modified to select
/// different run parameters, different material properties, etc; when this program is executed again (with
/// the same parameters), it loads the configuration files and uses the updated properties rather than the
/// default values.

#include "Sph.h"
#include "run/Collision.h"

using namespace Sph;

enum class CollisionParam {
    TARGET_RADIUS,
    TARGET_PERIOD,
    IMPACT_ENERGY,
    IMPACT_SPEED,
    IMPACT_ANGLE,
};

/// \brief Defines the command-line parameters of the program.
ArgsParser<CollisionParam> parser({ //
    { CollisionParam::TARGET_RADIUS, ArgEnum::FLOAT, OptionalEnum::MANDATORY },
    { CollisionParam::TARGET_PERIOD, ArgEnum::FLOAT, OptionalEnum::MANDATORY },
    { CollisionParam::IMPACT_ENERGY, ArgEnum::FLOAT, OptionalEnum::MANDATORY },
    { CollisionParam::IMPACT_SPEED, ArgEnum::FLOAT, OptionalEnum::MANDATORY },
    { CollisionParam::IMPACT_ANGLE, ArgEnum::FLOAT, OptionalEnum::MANDATORY } });

/// \brief Returns the name of the created output directory.
std::string getRunName(const Float targetRadius,
    const Float impactorRadius,
    const Float targetPeriod,
    const Float impactSpeed,
    const Float impactAngle) {
    std::stringstream ss;
    ss << "sph_" << round(targetRadius) << "m_" << round(impactorRadius) << "m_" << round(targetPeriod)
       << "h_" << round(impactSpeed) << "kms_" << round(impactAngle);
    return ss.str();
}

int main(int argc, char* argv[]) {
    FlatMap<CollisionParam, ArgValue> params;
    StdOutLogger logger;
    try {
        params = parser.parse(argc, argv);
    } catch (std::exception& e) {
        logger.write(e.what());
        logger.write("Usage:");
        logger.write(
            "launcher targetRadius[m] targetPeriod[h] impactEnergy[Q/Q*_D] impactSpeed[km/s] impactAngle[°]");
        return -1;
    }

    // convert the input units to SI
    Presets::CollisionParams cp;
    cp.targetRadius = params[CollisionParam::TARGET_RADIUS].get<float>();
    cp.targetRotation = 2._f * PI / (3600._f * params[CollisionParam::TARGET_PERIOD].get<float>());
    cp.impactSpeed = 1000._f * params[CollisionParam::IMPACT_SPEED].get<float>();
    cp.impactAngle = DEG_TO_RAD * params[CollisionParam::IMPACT_ANGLE].get<float>();

    // using specified impact energy, compute the necessary impact radius
    const Float impactEnergy = params[CollisionParam::IMPACT_ENERGY].get<float>();
    const Float density = BodySettings::getDefaults().get<Float>(BodySettingsId::DENSITY);
    cp.impactorRadius = getImpactorRadius(
        cp.targetRadius, cp.impactSpeed, cp.impactAngle, impactEnergy, density, EMPTY_FLAGS);

    // write parameters to the standard output
    logger.write("Target radius [m]:             ", cp.targetRadius);
    logger.write("Impactor radius [m]:           ", cp.impactorRadius);
    logger.write("Target period [h]:             ", 2._f * PI / (3600._f * cp.targetRotation));
    logger.write(
        "Critical period [h]:           ", 2._f * PI / (3600._f * computeCriticalFrequency(2700._f)));
    logger.write("Impact speed [km/s]:           ", cp.impactSpeed / 1000._f);
    logger.write("Impact angle [°]:              ", cp.impactAngle * RAD_TO_DEG);
    logger.write("Impact energy [Q/Q*_D]:        ", impactEnergy);

    const std::string runName = getRunName(cp.targetRadius,
        cp.impactorRadius,
        2._f * PI / (3600._f * cp.targetRotation),
        cp.impactSpeed / 1000._f,
        cp.impactAngle * RAD_TO_DEG);

    cp.outputPath = Path(runName);

    logger.write("Starting run ", runName);
    logger.write("");

    CollisionRun run(cp);
    run.setUp();
    run.run();

    return 0;
}
