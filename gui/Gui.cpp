#include "gui/Gui.h"
#include "geometry/Domain.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "gui/problems/Collision.h"
#include "gui/problems/Meteoroid.h"
#include "gui/windows/GlPane.h"
#include "gui/windows/OrthoPane.h"
#include "gui/windows/Window.h"
#include "physics/Constants.h"
#include "problem/Problem.h"
#include "sph/initial/Initial.h"
#include "sph/timestepping/TimeStepping.h"
#include "system/Factory.h"
#include "system/Logger.h"
#include "system/Output.h"

#include <wx/glcanvas.h>
#include <wx/sizer.h>
#include <wx/wx.h>


IMPLEMENT_APP(Sph::MyApp)

NAMESPACE_SPH_BEGIN

/*void MyApp::initialConditions(const GlobalSettings& globalSettings, const std::shared_ptr<Storage>& storage)
{
    BodySettings bodySettings;
    bodySettings.set(BodySettingsIds::ENERGY, 0._f);
    bodySettings.set(BodySettingsIds::ENERGY_RANGE, Range(0._f, INFTY));
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 10000);
    bodySettings.set(BodySettingsIds::EOS, EosEnum::TILLOTSON);
    bodySettings.set(BodySettingsIds::STRESS_TENSOR_MIN, 1.e10_f);
    InitialConditions conds(storage, globalSettings);

    StdOutLogger logger;
    SphericalDomain domain1(Vector(0._f), 5e3_f); // D = 10km
    conds.addBody(domain1, bodySettings);
    logger.write("Particles of target: ", storage->getParticleCnt());

    //    SphericalDomain domain2(Vector(4785.5_f, 3639.1_f, 0._f), 146.43_f); // D = 280m
    SphericalDomain domain2(Vector(3997.45_f, 3726.87_f, 0._f), 270.585_f);

    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100);
    conds.addBody(domain2, bodySettings, Vector(-5.e3_f, 0._f, 0._f)); // 5km/s
    logger.write("Particles in total: ", storage->getParticleCnt());
}*/

bool MyApp::OnInit() {
    /*GlobalSettings globalSettings;
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
        .set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-5_f)
        .set(GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP, 1.e-1_f)
        .set(GlobalSettingsIds::MODEL_FORCE_DIV_S, true)
        .set(GlobalSettingsIds::SPH_FINDER, FinderEnum::VOXEL)
        .set(GlobalSettingsIds::MODEL_AV_TYPE, ArtificialViscosityEnum::STANDARD)
        .set(GlobalSettingsIds::MODEL_DAMAGE, DamageEnum::SCALAR_GRADY_KIPP)
        .set(GlobalSettingsIds::MODEL_YIELDING, YieldingEnum::VON_MISES);
    Problem* p = new Problem(globalSettings, std::make_shared<Storage>());
    std::string outputDir = "out/" + globalSettings.get<std::string>(GlobalSettingsIds::RUN_OUTPUT_NAME);
    p->output = std::make_unique<TextOutput>(outputDir,
        globalSettings.get<std::string>(GlobalSettingsIds::RUN_NAME),
        Array<QuantityIds>{ QuantityIds::POSITIONS,
            QuantityIds::DENSITY,
            QuantityIds::PRESSURE,
            QuantityIds::ENERGY,
            QuantityIds::DAMAGE });
    // QuantityIds::DEVIATORIC_STRESS,
    // QuantityIds::RHO_GRAD_V });

    initialConditions(globalSettings, p->storage);

    GuiSettings guiSettings;
    guiSettings.set(GuiSettingsIds::VIEW_FOV, 1.e4_f)
        .set(GuiSettingsIds::PARTICLE_RADIUS, 0.3_f)
        .set(GuiSettingsIds::ORTHO_CUTOFF, INFTY) // 5.e2_f)
        .set(GuiSettingsIds::ORTHO_PROJECTION, OrthoEnum::XZ);*/


    // MeteoroidEntry setup;
    AsteroidCollision setup;

    p = setup.getProblem();
    GuiSettings guiSettings = setup.getGuiSettings();
    window = new Window(p->storage, guiSettings, [this, setup]() {
        ASSERT(this->worker.joinable());
        this->worker.join();
        p->storage->removeAll();
        setup.initialConditions(p->storage);
        this->worker = std::thread([this]() { p->run(); });
    });
    window->SetAutoLayout(true);
    window->Show();

    p->callbacks = std::make_unique<GuiCallbacks>(window);
    worker = std::thread([this]() { p->run(); });
    return true;
}

MyApp::MyApp() = default;

MyApp::~MyApp() {
    worker.join();
}

NAMESPACE_SPH_END
