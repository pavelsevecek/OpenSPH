#include "gui/Gui.h"
#include "gui/GlPane.h"
#include "gui/GuiCallbacks.h"
#include "gui/OrthoPane.h"
#include "gui/Settings.h"
#include "gui/Settings.h"
#include "gui/Window.h"
#include "physics/Constants.h"
#include "problem/Problem.h"
#include "sph/initial/Initial.h"
#include "system/Factory.h"

#include <wx/glcanvas.h>
#include <wx/sizer.h>
#include <wx/wx.h>


IMPLEMENT_APP(Sph::MyApp)

NAMESPACE_SPH_BEGIN

void MyApp::initialConditions(const GlobalSettings& globalSettings, const std::shared_ptr<Storage>& storage) {
    auto bodySettings = BODY_SETTINGS;
    bodySettings.set(BodySettingsIds::ENERGY, 0._f); /// \todo ok? shouldn't it be 1.e2_f);
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 10000);
    bodySettings.set(BodySettingsIds::EOS, EosEnum::TILLOTSON);
    InitialConditions conds(storage, globalSettings);

    SphericalDomain domain1(Vector(0._f), 5e3_f); // D = 10km
    conds.addBody(domain1, bodySettings);

    SphericalDomain domain2(Vector(3785.5_f, 3639.1_f, 0._f), 146.43_f); // D = 280m
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100);
    conds.addBody(domain2, bodySettings, Vector(-5.e3_f, 0._f, 0._f)); // 5km/s
}

bool MyApp::OnInit() {
    auto globalSettings = GLOBAL_SETTINGS;
    /*globalSettings.set(GlobalSettingsIds::DOMAIN_BOUNDARY, BoundaryEnum::GHOST_PARTICLES);
    globalSettings.set(GlobalSettingsIds::DOMAIN_RADIUS, 2.5_f);
    globalSettings.set(GlobalSettingsIds::DOMAIN_TYPE, DomainEnum::SPHERICAL);*/
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_ADAPTIVE, true);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-4_f);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP, 1.e-1_f);
    globalSettings.set(GlobalSettingsIds::MODEL_FORCE_DIV_S, true);
    globalSettings.set(GlobalSettingsIds::SPH_FINDER, FinderEnum::VOXEL);
    globalSettings.set(GlobalSettingsIds::RUN_OUTPUT_STEP, 100);
    globalSettings.set(GlobalSettingsIds::MODEL_DAMAGE, DamageEnum::SCALAR_GRADY_KIPP);
    globalSettings.set(GlobalSettingsIds::MODEL_YIELDING, YieldingEnum::VON_MISES);
    Problem* p = new Problem(globalSettings);
    p->timeRange = Range(0._f, 10._f);
    std::string outputDir = "out/" + globalSettings.get<std::string>(GlobalSettingsIds::RUN_OUTPUT_NAME);
    p->output = std::make_unique<TextOutput>(outputDir,
        globalSettings.get<std::string>(GlobalSettingsIds::RUN_NAME),
        Array<QuantityIds>{ QuantityIds::POSITIONS,
            QuantityIds::DENSITY,
            QuantityIds::PRESSURE,
            QuantityIds::ENERGY,
            QuantityIds::DEVIATORIC_STRESS,
            QuantityIds::RHO_GRAD_V });

    initialConditions(globalSettings, p->storage);
    p->timeStepping = Factory::getTimestepping(globalSettings, p->storage);

    GuiSettings guiSettings = GUI_SETTINGS;
    guiSettings.set<Float>(GuiSettingsIds::VIEW_FOV, 1.e4_f);
    guiSettings.set<Float>(GuiSettingsIds::PARTICLE_RADIUS, 0.5_f);
    guiSettings.set<Float>(GuiSettingsIds::ORTHO_CUTOFF, 1.e3_f);
    window = new Window(p->storage, guiSettings, [globalSettings, p, this]() {
        this->worker.join();
        p->storage->removeAll();
        this->initialConditions(globalSettings, p->storage);
        this->worker = std::thread([&p]() { p->run(); });
    });
    window->SetAutoLayout(true);
    window->Show();

    p->callbacks = std::make_unique<GuiCallbacks>(window);
    worker = std::thread([&p]() { p->run(); });
    return true;
}

MyApp::~MyApp() {
    worker.join();
}

NAMESPACE_SPH_END
