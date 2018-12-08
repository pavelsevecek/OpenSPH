#include "run/Collision.h"
#include "catch.hpp"
#include "io/FileSystem.h"
#include "io/Output.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("CollisionRun", "[run]") {

    Path dir("collision");
    Array<Path> files = {
        Path("collision.sph"),
        Path("fragmentation.sph"),
        Path("stabilization.sph"),
        Path("reaccumulation.sph"),
        Path("target.sph"),
        Path("impactor.sph"),
        Path("frag_final.ssf"),
        Path("reacc_final.ssf"),
    };

    for (Path file : files) {
        FileSystem::removePath(dir / file);
    }

    Presets::CollisionParams cp;
    cp.targetParticleCnt = 1000;
    cp.targetRadius = 1.e5_f;
    cp.impactorRadius = 1.e4_f;
    cp.impactAngle = 15._f * DEG_TO_RAD;
    cp.impactSpeed = 5.e3_f;
    cp.targetRotation = 2._f * PI / (3600._f * 6._f);

    PhaseParams pp;
    pp.outputPath = cp.outputPath = dir;
    pp.stab.range = Interval(0._f, 1._f);
    pp.stab.overrides.set(RunSettingsId::RUN_LOGGER, LoggerEnum::NONE);
    pp.frag.range = Interval(0._f, 1._f);
    pp.frag.overrides.set(RunSettingsId::RUN_LOGGER, LoggerEnum::NONE);
    pp.reacc.range = Interval(0._f, 1.e3_f);
    pp.reacc.overrides.set(RunSettingsId::RUN_LOGGER, LoggerEnum::NONE);

    CollisionRun first(cp, pp, makeShared<NullCallbacks>());

    REQUIRE_NOTHROW(first.setUp());
    first.run();

    for (Path file : files) {
        REQUIRE(FileSystem::pathExists(dir / file));
    }

    BinaryInput input;
    Expected<BinaryInput::Info> fragInfo = input.getInfo(dir / Path("frag_final.ssf"));
    REQUIRE(fragInfo);
    REQUIRE(fragInfo->runType == RunTypeEnum::SPH);

    Expected<BinaryInput::Info> reaccInfo = input.getInfo(dir / Path("reacc_final.ssf"));
    REQUIRE(reaccInfo);
    REQUIRE(reaccInfo->runType == RunTypeEnum::NBODY);

    // clear to be sure
    cp = Presets::CollisionParams{};
    REQUIRE(cp.loadFromFile(pp.outputPath / Path("collision.sph")));
    REQUIRE(cp.targetRotation == 2._f * PI / (3600._f * 6._f));
    REQUIRE(cp.impactAngle == 15._f * DEG_TO_RAD);

    // resume fragmentation
    CollisionRun resumeFrag(dir / Path("frag_0002.ssf"), pp, makeShared<NullCallbacks>());
    REQUIRE_NOTHROW(resumeFrag.setUp());
    resumeFrag.run();

    // resume reaccumulation
    CollisionRun resumeReacc(dir / Path("reacc_0002.ssf"), pp, makeShared<NullCallbacks>());
    REQUIRE_NOTHROW(resumeReacc.setUp());
    resumeReacc.run();
}
