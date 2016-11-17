#include "physics/Eos.h"
#include "problem/Problem.h"
#include "system/Factory.h"

using namespace Sph;

int main() {
    auto globalSettings = GLOBAL_SETTINGS;
    Problem<BasicModel<3>> p(globalSettings);
    p.logger       = std::make_unique<StdOutput>();
    p.timeRange    = Range<Float>(0._f, 10._f);
    p.timeStepping = Factory::getTimestepping(globalSettings, p.storage);

    auto bodySettings = BODY_SETTINGS;
    bodySettings.set(BodySettingsIds::ENERGY, 0.001_f);
    auto domain1  = std::make_unique<SphericalDomain>(Vector(0._f), 1._f);
    Storage body1 = p.model.createParticles(domain1.get(), bodySettings);

    auto domain2  = std::make_unique<SphericalDomain>(Vector(2._f, 1._f, 0._f), 0.3_f);
    Storage body2 = p.model.createParticles(domain2.get(), bodySettings);
    body2.dt<QuantityKey::R>().fill(Vector(-0.4_f, 0._f, 0._f));

    *p.storage = std::move(body1);
    p.storage->merge(body2);

    p.run();

    Profiler::getInstance()->printStatistics(p.logger.get());

    return 0;
}
