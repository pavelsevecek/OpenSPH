#include "sph/initial/Stellar.h"
#include "objects/geometry/Domain.h"
#include "objects/utility/IteratorAdapters.h"
#include "physics/Constants.h"
#include "physics/Eos.h"
#include "quantities/Quantity.h"
#include "sph/Materials.h"
#include "sph/initial/Distribution.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

Lut<Float> Stellar::solveLaneEmden(const Float n, const Float dz, const Float z_max) {
    Float phi = 1._f;
    Float dphi = 0._f;
    const Float z0 = 1.e-3_f;
    Float z = z0;
    Array<Float> solution;
    while (phi >= 0._f && z < z_max) {
        solution.push(phi);
        const Float d2phi = -2._f / z * dphi - pow(phi, n);
        dphi += d2phi * dz;
        phi += dphi * dz;
        z += dz;
    }
    return Lut<Float>(Interval(z0, z), std::move(solution));
}

/*Float Stellar::PolytropeParams::polytropeConstant() const {*/
/*    const Float R = Constants::gasConstant;
    const Float a = Constants::radiationDensity;
    return 1._f / beta * R / mu * cbrt((1._f - beta) / beta * 3._f * R / (mu * a));*/
/*return cbrt(3._f / PI) * Constants::planckConstant * Constants::speedOfLight /
       (8._f * pow(Constants::atomicMass, 4._f / 3._f));
}*/

Stellar::Star Stellar::polytropicStar(const Float radius, const Float mass, const Float n) {
    const Float G = Constants::gravity;

    Lut<Float> phi = solveLaneEmden(n);
    const Float z_star = phi.getRange().upper();
    const Float dphi_star = phi.derivative()(z_star);

    const Float rho_c = 3._f * mass / (4._f * PI * pow<3>(radius)) * z_star / (-3._f * dphi_star);
    const Float P_c = G * sqr(mass) / pow<4>(radius) * 1._f / (-4._f * PI * (n + 1) * z_star * dphi_star);

    IdealGasEos eos((n + 1._f) / n);
    Array<Float> rho(phi.size()), u(phi.size()), P(phi.size());
    for (auto el : iterateWithIndex(phi)) {
        const Size i = el.index();
        const Float x = el.value().y;
        rho[i] = rho_c * pow(x, n);
        P[i] = P_c * pow(x, n + 1);
        u[i] = eos.getInternalEnergy(rho[i], P[i]);
    }

    Star star;
    const Interval range(0._f, radius);
    star.p = Lut<Float>(range, std::move(P));
    star.rho = Lut<Float>(range, std::move(rho));
    star.u = Lut<Float>(range, std::move(u));

    return star;
}

Storage Stellar::generateIc(const RunSettings& globals,
    const Size N,
    const Float radius,
    const Float mass,
    const Float n) {
    SharedPtr<IScheduler> scheduler = Factory::getScheduler(globals);
    SphericalDomain domain(Vector(0._f), radius);
    ParametrizedSpiralingDistribution distr(1234);
    Array<Vector> points = distr.generate(*scheduler, N, domain);

    Star star = polytropicStar(radius, mass, n);

    Array<Float> m(points.size());
    Array<Float> rho(points.size());
    Array<Float> u(points.size());
    const Float v = domain.getVolume() / points.size();
    for (Size i = 0; i < points.size(); ++i) {
        const Float r = getLength(points[i]);
        rho[i] = star.rho(r);
        u[i] = star.u(r);
        m[i] = rho[i] * v;
    }

    BodySettings body;
    body.set(BodySettingsId::EOS, EosEnum::IDEAL_GAS);
    body.set(BodySettingsId::ADIABATIC_INDEX, (n + 1._f) / n);
    body.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE);
    body.set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE);

    /// \todo add to params
    const Float eta = body.get<Float>(BodySettingsId::SMOOTHING_LENGTH_ETA);
    for (Size i = 0; i < points.size(); ++i) {
        points[i][H] *= eta;
    }

    Storage storage(makeShared<EosMaterial>(body));

    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(points));
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, std::move(m));
    storage.insert<Float>(QuantityId::ENERGY, OrderEnum::ZERO, std::move(u));
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, std::move(rho));

    MaterialInitialContext context(globals);
    storage.getMaterial(0)->create(storage, context);

    return storage;
}

NAMESPACE_SPH_END
