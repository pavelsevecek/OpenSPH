#include "physics/Eos.h"
#include "problem/Problem.h"
#include "system/Factory.h"



using namespace Sph;

int main() {
    auto globalSettings = GLOBAL_SETTINGS;
    globalSettings.set<int>(GlobalSettingsIds::FINDER, int(FinderEnum::BRUTE_FORCE));
    Problem<BasicModel>* p = new Problem<BasicModel>(globalSettings);
    p->logger              = std::make_unique<StdOut>();
    p->timeRange           = Range<Float>(0._f, 1000._f);
    p->timestepping        = Factory::getTimestepping(globalSettings, p->storage);

    auto bodySettings = BODY_SETTINGS;
    bodySettings.set(BodySettingsIds::ENERGY, 0.001_f);
    Storage body1 =
        p->model.createParticles(1000, std::make_unique<SphericalDomain>(Vector(0._f), 1._f), bodySettings);

    Storage body2 =
        p->model.createParticles(100,
                                 std::make_unique<SphericalDomain>(Vector(2._f, 1._f, 0._f), 0.3_f),
                                 bodySettings);
    body2.dt<QuantityKey::R>().fill(Vector(-0.4_f, 0._f, 0._f));

    *p->storage = std::move(body1);
    p->storage->merge(body2);


    p->run();
    return 0;
}
