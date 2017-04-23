#include "gui/problems/Collision.h"
#include "geometry/Domain.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "sph/initial/Initial.h"

NAMESPACE_SPH_BEGIN

AsteroidCollision::AsteroidCollision() {
    settings.set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.01_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 0.01_f)
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 0.1_f)
        .set(RunSettingsId::MODEL_FORCE_DIV_S, true)
        .set(RunSettingsId::SPH_FINDER, FinderEnum::VOXEL)
        .set(RunSettingsId::MODEL_AV_TYPE, ArtificialViscosityEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
        .set(RunSettingsId::SPH_AV_BETA, 3._f);
    settings.saveToFile("code.sph");

    storage = std::make_shared<Storage>();
    GuiSettings gui = this->getGuiSettings();
    window = new Window(storage, gui /*, [this, setup]() mutable {
        ASSERT(this->worker.joinable());
        this->worker.join();
        p->storage->removeAll();
        setup.initialConditions(p->storage);
        this->worker = std::thread([this]() { p->run(); });
    }*/);
    window->SetAutoLayout(true);
    window->Show();
}

GuiSettings AsteroidCollision::getGuiSettings() const {
    GuiSettings guiSettings;
    guiSettings.set(GuiSettingsId::VIEW_FOV, 5.e3_f)
        .set(GuiSettingsId::VIEW_CENTER, Vector(320, 200, 0._f))
        .set(GuiSettingsId::PARTICLE_RADIUS, 0.3_f)
        .set(GuiSettingsId::ORTHO_CUTOFF, 5.e2_f)
        .set(GuiSettingsId::ORTHO_PROJECTION, OrthoEnum::XY)
        .set(GuiSettingsId::IMAGES_SAVE, true)
        .set(GuiSettingsId::IMAGES_TIMESTEP, 0.02_f);
    return guiSettings;
}

void AsteroidCollision::setUp() {
    BodySettings bodySettings;
    bodySettings.set(BodySettingsId::ENERGY, 1._f)
        .set(BodySettingsId::ENERGY_RANGE, Range(1._f, INFTY))
        .set(BodySettingsId::PARTICLE_COUNT, 100000)
        .set(BodySettingsId::EOS, EosEnum::TILLOTSON)
        .set(BodySettingsId::STRESS_TENSOR_MIN, 1.e6_f)
        .set(BodySettingsId::RHEOLOGY_DAMAGE, DamageEnum::SCALAR_GRADY_KIPP)
        .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES);
    bodySettings.saveToFile("target.sph");

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
    output = std::make_unique<TextOutput>(
        outputDir, settings.get<std::string>(RunSettingsId::RUN_NAME), TextOutput::Options::SCIENTIFIC);
    output->add(std::make_unique<ParticleNumberColumn>());
    output->add(Factory::getValueColumn<Vector>(QuantityId::POSITIONS));
    output->add(Factory::getDerivativeColumn<Vector>(QuantityId::POSITIONS));
    output->add(Factory::getSmoothingLengthColumn());
    output->add(Factory::getValueColumn<Float>(QuantityId::DENSITY));
    output->add(Factory::getValueColumn<Float>(QuantityId::PRESSURE));
    output->add(Factory::getValueColumn<Float>(QuantityId::ENERGY));
    output->add(Factory::getValueColumn<Float>(QuantityId::DAMAGE));
    output->add(Factory::getValueColumn<TracelessTensor>(QuantityId::DEVIATORIC_STRESS));

    logFiles.push(std::make_unique<EnergyLogFile>("energy.txt"));
    logFiles.push(std::make_unique<TimestepLogFile>("timestep.txt"));

    callbacks = std::make_unique<GuiCallbacks>(window, this->getGuiSettings());
}

NAMESPACE_SPH_END
