#pragma once


#include "gui/Settings.h"
#include "physics/Eos.h"
#include "problem/Problem.h"
#include "quantities/Storage.h"
#include "sph/initial/Initial.h"
#include "system/Logger.h"
#include "system/Output.h"

NAMESPACE_SPH_BEGIN

/// \todo problems should be setup by inheriting Abstract::Problem. This interface should have something like
/// setGlobalSettings, and setInitialConditions = 0.
struct MeteoroidEntry {
    GlobalSettings globalSettings;

    void initialConditions(const std::shared_ptr<Storage>& storage) const {
        BodySettings bodySettings;
        bodySettings.set(BodySettingsIds::DENSITY, 1._f);
        bodySettings.set(BodySettingsIds::DENSITY_RANGE, Range(1.e-3_f, 1.e3_f));
        bodySettings.set(BodySettingsIds::ENERGY, IdealGasEos(1.4_f).getInternalEnergy(1._f, 1.e5_f));
        bodySettings.set(BodySettingsIds::ENERGY_RANGE, Range(0._f, INFTY));
        bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 10000);
        bodySettings.set(BodySettingsIds::EOS, EosEnum::IDEAL_GAS);
        bodySettings.set(BodySettingsIds::SHEAR_MODULUS, 0._f);
        InitialConditions conds(storage, globalSettings);

        StdOutLogger logger;
        CylindricalDomain domain(Vector(0._f), 1._f, 2._f, true);
        conds.addBody(domain, bodySettings, Vector(0._f, 0._f, -20._f));
        logger.write("Particles of target: ", storage->getParticleCnt());
    }

    std::unique_ptr<Problem> getProblem() {
        globalSettings.set(GlobalSettingsIds::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::EULER_EXPLICIT)
            .set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-5_f)
            .set(GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP, 1._f)
            .set(GlobalSettingsIds::MODEL_FORCE_DIV_S, false)
            .set(GlobalSettingsIds::SPH_FINDER, FinderEnum::VOXEL)
            .set(GlobalSettingsIds::MODEL_AV_TYPE, ArtificialViscosityEnum::STANDARD)
            .set(GlobalSettingsIds::MODEL_AV_BALSARA_SWITCH, false)
            .set(GlobalSettingsIds::MODEL_DAMAGE, DamageEnum::NONE)
            .set(GlobalSettingsIds::MODEL_YIELDING, YieldingEnum::NONE)
            .set(GlobalSettingsIds::DOMAIN_BOUNDARY, BoundaryEnum::WIND_TUNNEL)
            .set(GlobalSettingsIds::DOMAIN_TYPE, DomainEnum::CYLINDER)
            .set(GlobalSettingsIds::DOMAIN_RADIUS, 1._f)
            .set(GlobalSettingsIds::DOMAIN_HEIGHT, 2._f);
        std::unique_ptr<Problem> p = std::make_unique<Problem>(globalSettings, std::make_shared<Storage>());
        std::string outputDir = "out/" + globalSettings.get<std::string>(GlobalSettingsIds::RUN_OUTPUT_NAME);
        p->output = std::make_unique<TextOutput>(outputDir,
            globalSettings.get<std::string>(GlobalSettingsIds::RUN_NAME),
            Array<QuantityIds>{
                QuantityIds::POSITIONS, QuantityIds::DENSITY, QuantityIds::PRESSURE, QuantityIds::ENERGY });

        initialConditions(p->storage);
        return p;
    }

    GuiSettings getGuiSettings() const {
        GuiSettings guiSettings;
        guiSettings.set(GuiSettingsIds::VIEW_FOV, 1.5_f)
            .set(GuiSettingsIds::PARTICLE_RADIUS, 0.3_f)
            .set(GuiSettingsIds::ORTHO_CUTOFF, 0.3_f) // 5.e2_f)
            .set(GuiSettingsIds::ORTHO_PROJECTION, OrthoEnum::XZ);
        return guiSettings;
    }
};

NAMESPACE_SPH_END
