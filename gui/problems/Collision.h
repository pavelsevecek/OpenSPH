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
        bodySettings.set(BodySettingsIds::ENERGY, 0._f);
        bodySettings.set(BodySettingsIds::ENERGY_RANGE, Range(0._f, INFTY));
        bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 400);
        bodySettings.set(BodySettingsIds::EOS, EosEnum::TILLOTSON);
        bodySettings.set(BodySettingsIds::STRESS_TENSOR_MIN, 1.e6_f);
        InitialConditions conds(storage, globalSettings);

        StdOutLogger logger;
        SphericalDomain domain1(Vector(0._f), 5e3_f); // D = 10km
        conds.addBody(domain1, bodySettings);
        logger.write("Particles of target: ", storage->getParticleCnt());

        //    SphericalDomain domain2(Vector(4785.5_f, 3639.1_f, 0._f), 146.43_f); // D = 280m
        SphericalDomain domain2(Vector(3997.45_f, 3726.87_f, 0._f), 270.585_f);

        bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 10);
        conds.addBody(domain2, bodySettings, Vector(-5.e3_f, 0._f, 0._f)); // 5km/s
        logger.write("Particles in total: ", storage->getParticleCnt());
    }

    std::unique_ptr<Problem> getProblem() {
        globalSettings.set(GlobalSettingsIds::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
            .set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-5_f)
            .set(GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP, 1.e-1_f)
            .set(GlobalSettingsIds::MODEL_FORCE_DIV_S, true)
            .set(GlobalSettingsIds::SPH_FINDER, FinderEnum::VOXEL)
            .set(GlobalSettingsIds::MODEL_AV_TYPE, ArtificialViscosityEnum::STANDARD)
            .set(GlobalSettingsIds::MODEL_DAMAGE, DamageEnum::SCALAR_GRADY_KIPP)
            .set(GlobalSettingsIds::MODEL_YIELDING, YieldingEnum::VON_MISES);
        std::unique_ptr<Problem> p = std::make_unique<Problem>(globalSettings, std::make_shared<Storage>());
        std::string outputDir = "out/" + globalSettings.get<std::string>(GlobalSettingsIds::RUN_OUTPUT_NAME);
        p->output = std::make_unique<TextOutput>(outputDir,
            globalSettings.get<std::string>(GlobalSettingsIds::RUN_NAME),
            Array<QuantityIds>{
                QuantityIds::POSITIONS, QuantityIds::DENSITY, QuantityIds::PRESSURE, QuantityIds::ENERGY });
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
