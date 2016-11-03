#include "geometry/vector.h"
#include "core/core.h"
#include "physics/eos.h"
#include "system/settings.h"

#include <iostream>

using namespace Sph;

int main() {
    //globalSettings.saveToFile("/home/pavel/projects/astro/sph2/settings.conf");

    Problem<Body> p1;
//    p1.domain = std::make_unique<SphericalDomain>(Vector(0._f), 1._f);
    p1.finder = std::make_unique<KdTree>();
    p1.logger = std::make_unique<StdOut>();
    p1.timestepping = std::make_unique<EulerExplicit>(0.1_f);
    p1.timeRange = Range<Float>(0._f, 10._f);
    p1.model.create(10000, new SphericalDomain(Vector(0._f), 1._f), new HexagonalPacking(), BODY_SETTINGS);

    Body projectile;
    projectile.create(1000, new SphericalDomain(Vector(2._f, 1._f, 0._f), 0.3_f), new HexagonalPacking(), BODY_SETTINGS);
    projectile.addVelocity(Vector(-3.f, 0.f, 0.f));
    p1.model.merge(projectile);

    //p1.model.saveToFile("/home/pavel/projects/astro/sph2/dump.txt");

    p1.run();
    return 0;
}
