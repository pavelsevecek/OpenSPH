#include "io/Logger.h"
#include "physics/Functions.h"
#include "run/Collision.h"
#include "sph/initial/Presets.h"
#include "system/ArgsParser.h"

using namespace Sph;

enum class CollisionParam {
    TARGET_RADIUS,
    TARGET_PERIOD,
    IMPACT_ENERGY,
    IMPACT_SPEED,
    IMPACT_ANGLE,
};

ArgsParser<CollisionParam> parser({ //
    { CollisionParam::TARGET_RADIUS, ArgEnum::FLOAT, OptionalEnum::MANDATORY },
    { CollisionParam::TARGET_PERIOD, ArgEnum::FLOAT, OptionalEnum::MANDATORY },
    { CollisionParam::IMPACT_ENERGY, ArgEnum::FLOAT, OptionalEnum::MANDATORY },
    { CollisionParam::IMPACT_SPEED, ArgEnum::FLOAT, OptionalEnum::MANDATORY },
    { CollisionParam::IMPACT_ANGLE, ArgEnum::FLOAT, OptionalEnum::MANDATORY } });


std::string getRunName(const Float targetRadius,
    const Float impactorRadius,
    const Float targetPeriod,
    const Float impactSpeed,
    const Float impactAngle) {
    std::stringstream ss;
    ss << "sph_" << int(targetRadius) << "m_" << int(impactorRadius) << "m_" << int(targetPeriod) << "h_"
       << int(impactSpeed) << "kms_" << int(impactAngle);
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
        return 0;
    }

    Presets::CollisionParams cp;
    cp.targetRadius = params[CollisionParam::TARGET_RADIUS].get<float>();
    cp.targetRotation = params[CollisionParam::TARGET_PERIOD].get<float>();
    cp.impactSpeed = params[CollisionParam::IMPACT_SPEED].get<float>();
    cp.impactAngle = params[CollisionParam::IMPACT_ANGLE].get<float>();

    const Float impactEnergy = params[CollisionParam::IMPACT_ENERGY].get<float>();
    cp.impactorRadius =
        0.5_f * getImpactorDiameter(2._f * cp.targetRadius, 2700._f, 1000._f * cp.impactSpeed, impactEnergy);

    logger.write("Target radius [m]:      ", cp.targetRadius);
    logger.write("Impactor radius [m]:    ", cp.impactorRadius);
    logger.write("Target period [h]:      ", cp.targetRotation);
    logger.write("Impact speed [km/s]:    ", cp.impactSpeed);
    logger.write("Impact angle [°]:       ", cp.impactAngle);
    logger.write("Impact energy [Q/Q*_D]: ", impactEnergy);

    const std::string runName =
        getRunName(cp.targetRadius, cp.impactorRadius, cp.targetRotation, cp.impactSpeed, cp.impactAngle);
    logger.write("Starting run ", runName);
    logger.write("");

    // convert to correct units
    cp.targetRotation = 2._f * PI / (3600._f * cp.targetRotation);
    cp.impactSpeed = 1000._f * cp.impactSpeed;
    cp.impactAngle = DEG_TO_RAD * cp.impactAngle;
    cp.outputPath = Path(runName);
    // cp.targetParticleCnt = 1000;

    CollisionRun run(cp);
    run.setUp();
    run.run();
    return 0;
}
