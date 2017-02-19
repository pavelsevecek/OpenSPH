#pragma once

/// Asteroid collision problem setup
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "gui/Settings.h"
#include "problem/Problem.h"
#include "quantities/Storage.h"
#include "sph/initial/Initial.h"
#include "system/Logger.h"
#include "system/Output.h"

NAMESPACE_SPH_BEGIN

/// \todo problems should be setup by inheriting Abstract::Problem. This interface should have something like
/// setGlobalSettings, and setInitialConditions = 0.
struct AsteroidCollision {
    GlobalSettings globalSettings;

    void initialConditions(const std::shared_ptr<Storage>& storage) const {
        BodySettings bodySettings;
        bodySettings.set(BodySettingsIds::ENERGY, 1._f)
            .set(BodySettingsIds::ENERGY_RANGE, Range(1._f, INFTY))
            .set(BodySettingsIds::PARTICLE_COUNT, 1000)
            .set(BodySettingsIds::EOS, EosEnum::IDEAL_GAS)
            .set(BodySettingsIds::SHEAR_MODULUS, 0._f);
        InitialConditions conds(storage, globalSettings);

        StdOutLogger logger;
        SphericalDomain domain1(Vector(0._f), 5e3_f); // D = 10km
        conds.addBody(domain1, bodySettings);
        logger.write("Particles of target: ", storage->getParticleCnt());
        const Size n1 = storage->getParticleCnt();

        //    SphericalDomain domain2(Vector(4785.5_f, 3639.1_f, 0._f), 146.43_f); // D = 280m
        SphericalDomain domain2(Vector(5097.45_f, 3726.87_f, 0._f), 270.585_f);

        bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100);
        conds.addBody(domain2, bodySettings, Vector(-5.e3_f, 0._f, 0._f)); // 5km/s
        logger.write("Particles of projectile: ", storage->getParticleCnt() - n1);
    }

    std::unique_ptr<Problem> getProblem() {
        globalSettings.set(GlobalSettingsIds::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
            .set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 5.e-3_f)
            .set(GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP, 1.e-1_f)
            .set(GlobalSettingsIds::RUN_OUTPUT_INTERVAL, 0._f)
            .set(GlobalSettingsIds::RUN_TIMESTEP_CNT, 1000)
            .set(GlobalSettingsIds::MODEL_FORCE_DIV_S, true)
            .set(GlobalSettingsIds::SPH_FINDER, FinderEnum::VOXEL)
            .set(GlobalSettingsIds::MODEL_AV_TYPE, ArtificialViscosityEnum::STANDARD)
            .set(GlobalSettingsIds::SPH_AV_ALPHA, 1.5_f)
            .set(GlobalSettingsIds::SPH_AV_BETA, 3._f)
            .set(GlobalSettingsIds::MODEL_DAMAGE, DamageEnum::NONE)      // DamageEnum::SCALAR_GRADY_KIPP)
            .set(GlobalSettingsIds::MODEL_YIELDING, YieldingEnum::NONE); // YieldingEnum::VON_MISES);
        std::unique_ptr<Problem> p = std::make_unique<Problem>(globalSettings, std::make_shared<Storage>());
        std::string outputDir = "out/" + globalSettings.get<std::string>(GlobalSettingsIds::RUN_OUTPUT_NAME);
        p->output = std::make_unique<TextOutput>(
            outputDir, globalSettings.get<std::string>(GlobalSettingsIds::RUN_NAME));
        p->output->add(std::make_unique<ParticleNumberElement>());
        p->output->add(Factory::getValueElement<Vector>(QuantityIds::POSITIONS));
        p->output->add(Factory::getValueElement<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS));
        p->output->add(Factory::getDerivativeElement<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS));
        // Array<QuantityIds>{
        // QuantityIds::POSITIONS, QuantityIds::DENSITY, QuantityIds::PRESSURE, QuantityIds::ENERGY });
        //  QuantityIds::DAMAGE });
        // QuantityIds::DEVIATORIC_STRESS,
        // QuantityIds::RHO_GRAD_V });

        initialConditions(p->storage);
        return p;
    }

    GuiSettings getGuiSettings() const {
        GuiSettings guiSettings;
        guiSettings.set(GuiSettingsIds::VIEW_FOV, 1.e4_f)
            .set(GuiSettingsIds::PARTICLE_RADIUS, 0.3_f)
            .set(GuiSettingsIds::ORTHO_CUTOFF, 5.e2_f)
            .set(GuiSettingsIds::ORTHO_PROJECTION, OrthoEnum::XY);
        return guiSettings;
    }
};

NAMESPACE_SPH_END
