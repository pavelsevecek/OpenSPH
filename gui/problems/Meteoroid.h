#pragma once


#include "gui/Settings.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "physics/Eos.h"
#include "quantities/Storage.h"
#include "run/Run.h"
#include "sph/initial/Initial.h"

NAMESPACE_SPH_BEGIN

class MeteoroidEntry : public Abstract::Run {
public:
    MeteoroidEntry() {
        settings.set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::EULER_EXPLICIT)
            .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-5_f)
            .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 1._f)
            .set(RunSettingsId::MODEL_FORCE_SOLID_STRESS, false)
            .set(RunSettingsId::SPH_FINDER, FinderEnum::VOXEL)
            .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
            .set(RunSettingsId::SPH_AV_BALSARA, false)
            .set(RunSettingsId::DOMAIN_BOUNDARY, BoundaryEnum::WIND_TUNNEL)
            .set(RunSettingsId::DOMAIN_TYPE, DomainEnum::CYLINDER)
            .set(RunSettingsId::DOMAIN_RADIUS, 1._f)
            .set(RunSettingsId::DOMAIN_HEIGHT, 2._f);
        Path outputDir = Path("out") / Path(settings.get<std::string>(RunSettingsId::RUN_OUTPUT_NAME));
        output = makeAuto<TextOutput>(
            outputDir, settings.get<std::string>(RunSettingsId::RUN_NAME), TextOutput::Options::SCIENTIFIC);
    }


    virtual void setUp() override {
        BodySettings bodySettings;
        bodySettings.set(BodySettingsId::DENSITY, 1._f)
            .set(BodySettingsId::DENSITY_RANGE, Range(1.e-3_f, 1.e3_f))
            .set(BodySettingsId::ENERGY, IdealGasEos(1.4_f).getInternalEnergy(1._f, 1.e5_f))
            .set(BodySettingsId::ENERGY_RANGE, Range(0._f, INFTY))
            .set(BodySettingsId::PARTICLE_COUNT, 10000)
            .set(BodySettingsId::EOS, EosEnum::TILLOTSON)
            .set(BodySettingsId::RHEOLOGY_DAMAGE, DamageEnum::NONE)
            .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE)
            .set(BodySettingsId::SHEAR_MODULUS, 0._f);
        InitialConditions conds(*storage, settings);

        StdOutLogger logger;
        CylindricalDomain domain(Vector(0._f), 1._f, 2._f, true);
        conds.addBody(domain, bodySettings, Vector(0._f, 0._f, -20._f));
        logger.write("Particles of target: ", storage->getParticleCnt());
    }

    /*    virtual GuiSettings getGuiSettings() const override {
            GuiSettings guiSettings;
            guiSettings.set(GuiSettingsId::VIEW_FOV, 1.5_f)
                .set(GuiSettingsId::PARTICLE_RADIUS, 0.3_f)
                .set(GuiSettingsId::ORTHO_CUTOFF, 0.3_f) // 5.e2_f)
                .set(GuiSettingsId::ORTHO_PROJECTION, OrthoEnum::XZ);
            return guiSettings;
        }*/

    virtual void tearDown() override {}
};

NAMESPACE_SPH_END
