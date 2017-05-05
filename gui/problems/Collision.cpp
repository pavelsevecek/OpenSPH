#include "gui/problems/Collision.h"
#include "geometry/Domain.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "io/Column.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "sph/initial/Initial.h"

NAMESPACE_SPH_BEGIN

AsteroidCollision::AsteroidCollision(Controller* model)
    : model(model) {
    settings.set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.01_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 0.01_f)
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 0.1_f)
        .set(RunSettingsId::MODEL_FORCE_SOLID_STRESS, true)
        .set(RunSettingsId::SPH_FINDER, FinderEnum::VOXEL)
        .set(RunSettingsId::MODEL_AV_TYPE, ArtificialViscosityEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
        .set(RunSettingsId::SPH_AV_BETA, 3._f)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 1000);
    settings.saveToFile("code.sph");
}

SharedPtr<Storage> AsteroidCollision::setUp() {
    BodySettings bodySettings;
    bodySettings.set(BodySettingsId::ENERGY, 1._f)
        .set(BodySettingsId::ENERGY_RANGE, Range(1._f, INFTY))
        .set(BodySettingsId::PARTICLE_COUNT, 1'000'000)
        .set(BodySettingsId::EOS, EosEnum::TILLOTSON)
        .set(BodySettingsId::STRESS_TENSOR_MIN, 1.e6_f)
        .set(BodySettingsId::RHEOLOGY_DAMAGE, DamageEnum::SCALAR_GRADY_KIPP)
        .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES);
    bodySettings.saveToFile("target.sph");

    storage = makeShared<Storage>();
    InitialConditions conds(*storage, settings);

    StdOutLogger logger;
    SphericalDomain domain1(Vector(0._f), 5e3_f); // D = 10km
    conds.addBody(domain1, bodySettings);
    /// \todo save also problem-specific settings: position of impactor, radius, ...
    logger.write("Particles of target: ", storage->getParticleCnt());
    const Size n1 = storage->getParticleCnt();

    //    SphericalDomain domain2(Vector(4785.5_f, 3639.1_f, 0._f), 146.43_f); // D = 280m
    SphericalDomain domain2(Vector(5097.4509902022_f, 3726.8662269290_f, 0._f), 270.5847632732_f);

    bodySettings.set(BodySettingsId::PARTICLE_COUNT, 100).set(BodySettingsId::STRESS_TENSOR_MIN, LARGE);
    bodySettings.saveToFile("impactor.sph");
    conds.addBody(domain2, bodySettings, Vector(-5.e3_f, 0._f, 0._f)); // 5km/s
    logger.write("Particles of projectile: ", storage->getParticleCnt() - n1);

    std::string outputDir = "out/" + settings.get<std::string>(RunSettingsId::RUN_OUTPUT_NAME);
    output = makeAuto<TextOutput>(
        outputDir, settings.get<std::string>(RunSettingsId::RUN_NAME), TextOutput::Options::SCIENTIFIC);
    output->add(makeAuto<ParticleNumberColumn>());
    output->add(makeAuto<ValueColumn<Vector>>(QuantityId::POSITIONS));
    output->add(makeAuto<DerivativeColumn<Vector>>(QuantityId::POSITIONS));
    output->add(makeAuto<SmoothingLengthColumn>());
    output->add(makeAuto<ValueColumn<Float>>(QuantityId::DENSITY));
    output->add(makeAuto<ValueColumn<Float>>(QuantityId::PRESSURE));
    output->add(makeAuto<ValueColumn<Float>>(QuantityId::ENERGY));
    output->add(makeAuto<ValueColumn<Float>>(QuantityId::DAMAGE));
    output->add(makeAuto<ValueColumn<TracelessTensor>>(QuantityId::DEVIATORIC_STRESS));

    logFiles.push(makeAuto<EnergyLogFile>("energy.txt"));
    logFiles.push(makeAuto<TimestepLogFile>("timestep.txt"));

    callbacks = makeAuto<GuiCallbacks>(model);

    return storage;
}

NAMESPACE_SPH_END
