#include "Sph.h"
//#include "run/Presets.h"

using namespace Sph;

/// \brief Returns the name of the created output directory.
std::string getRunName(const Float targetRadius,
    const Float impactorRadius,
    const Float targetPeriod,
    const Float impactSpeed,
    const Float impactAngle) {
    std::stringstream ss;
    ss << "sph_" << round(targetRadius) << "m_" << round(impactorRadius) << "m_" << round(targetPeriod * 60)
       << "min_" << round(impactSpeed) << "kms_" << round(impactAngle);
    return ss.str();
}

int main(int, char** argv) {
    (void)argv;
    /*CollisionParams cp;

    // fixed parameters
    cp.geometry.set(CollisionGeometrySettingsId::TARGET_PARTICLE_COUNT, 250000);

    const Float impactSpeed = 5.e3_f;
    cp.geometry.set(CollisionGeometrySettingsId::IMPACT_SPEED, impactSpeed);

    ThreadPool pool;

    const int impactAngle1 = atoi(argv[1]);
    for (Float targetRadius : { 5.e4f }) {
        cp.geometry.set(CollisionGeometrySettingsId::TARGET_RADIUS, targetRadius);

        for (Float impactAngle : { impactAngle1 }) {
            cp.geometry.set(CollisionGeometrySettingsId::IMPACT_ANGLE, impactAngle);

            for (Float period : { 2._f, 3._f, 1000._f }) {
                cp.geometry.set(CollisionGeometrySettingsId::TARGET_SPIN_RATE, 24._f / period);

                for (Float impactEnergy : { 0.1_f, 0.3_f, 1._f, 3._f }) {
                    const Float density = BodySettings::getDefaults().get<Float>(BodySettingsId::DENSITY);
                    const Float impactorRadius =
                        getImpactorRadius(targetRadius, impactSpeed, impactEnergy, density);
                    cp.geometry.set(CollisionGeometrySettingsId::IMPACTOR_RADIUS, impactorRadius);

                    const std::string runName =
                        getRunName(targetRadius, impactorRadius, period, impactSpeed / 1000._f, impactAngle);

                    cp.outputPath = Path(runName);
                    SPH_ASSERT(!FileSystem::pathExists(cp.outputPath));

                    PhaseParams pp;
                    pp.outputPath = Path(runName);

                    const Float runTime = 3600._f * targetRadius / 5.e3_f; // 1 hour for 10km body
                    pp.stab.range = Interval(0._f, 10._f * targetRadius / 5.e3);
                    pp.stab.overrides.set(RunSettingsId::RUN_LOGGER, LoggerEnum::FILE)
                        .set(RunSettingsId::RUN_LOGGER_FILE, std::string(runName + "/stab.log"))
                        .set(RunSettingsId::RUN_THREAD_CNT, 1);

                    pp.frag.range = Interval(0._f, runTime);
                    pp.frag.overrides.set(RunSettingsId::RUN_LOGGER, LoggerEnum::FILE)
                        .set(RunSettingsId::RUN_LOGGER_FILE, std::string(runName + "/frag.log"))
                        .set(RunSettingsId::RUN_THREAD_CNT, 1);

                    pp.reacc.range = Interval(0._f, 3600._f * 24._f * 10._f); // 10 days
                    pp.reacc.overrides.set(RunSettingsId::COLLISION_BOUNCE_MERGE_LIMIT, 1._f)
                        .set(RunSettingsId::COLLISION_OVERLAP, OverlapEnum::FORCE_MERGE)
                        .set(RunSettingsId::COLLISION_HANDLER, CollisionHandlerEnum::PERFECT_MERGING)
                        .set(RunSettingsId::RUN_LOGGER, LoggerEnum::FILE)
                        .set(RunSettingsId::RUN_LOGGER_FILE, std::string(runName + "/reac.log"))
                        .set(RunSettingsId::RUN_THREAD_CNT, 1);

                    pool.submit([cp, pp] {
                        CollisionRun run(cp, pp, makeShared<NullCallbacks>());
                        run.setUp();
                        run.run();
                      //  runCollision(Path{});
                    });
                }
            }
        }
    }

    pool.waitForAll();*/

    return 0;
}
