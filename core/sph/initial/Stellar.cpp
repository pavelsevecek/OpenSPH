#include "sph/initial/Stellar.h"
#include "objects/geometry/Domain.h"
#include "objects/utility/IteratorAdapters.h"
#include "physics/Constants.h"
#include "physics/Eos.h"
#include "quantities/Quantity.h"
#include "sph/Materials.h"
#include "sph/initial/Distribution.h"
#include "system/Factory.h"
#include "thread/Scheduler.h"

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

Stellar::Star Stellar::polytropicStar(const IEos& eos, const Float radius, const Float mass, const Float n) {
    const Float G = Constants::gravity;

    Lut<Float> phi = solveLaneEmden(n);
    const Float z_star = phi.getRange().upper();
    const Float dphi_star = phi.derivative()(z_star);

    const Float rho_avg = mass / sphereVolume(radius);
    const Float rho_c = rho_avg * z_star / (-3._f * dphi_star);
    const Float P_c = G * sqr(mass) / pow<4>(radius) * 1._f / (4._f * PI * (n + 1) * sqr(dphi_star));

    Array<Float> rho(phi.size()), u(phi.size()), P(phi.size());
    for (auto el : iterateWithIndex(phi)) {
        const Size i = el.index();
        const Float x = el.value().y;
        rho[i] = rho_c * pow(x, n);
        P[i] = P_c * pow(x, n + 1);
        u[i] = eos.getInternalEnergy(rho[i], P[i]);

        SPH_ASSERT(rho[i] > 0 && P[i] > 0, rho[i], P[i]);
    }

    Star star;
    const Interval range(0._f, radius);
    star.p = Lut<Float>(range, std::move(P));
    star.rho = Lut<Float>(range, std::move(rho));
    star.u = Lut<Float>(range, std::move(u));

    return star;
}

Storage Stellar::generateIc(const SharedPtr<IScheduler>& scheduler,
    const SharedPtr<IMaterial>& material,
    const IDistribution& distribution,
    const Size N,
    const Float radius,
    const Float mass) {
    SphericalDomain domain(Vector(0._f), radius);
    Array<Vector> points = distribution.generate(*scheduler, N, domain);

    const RawPtr<EosMaterial> eos = dynamicCast<EosMaterial>(material.get());
    if (!eos) {
        throw Exception("Cannot generate IC without equation of state");
    }
    const Float gamma = material->getParam<Float>(BodySettingsId::ADIABATIC_INDEX);
    const Float n = 1._f / (gamma - 1._f);
    Star star = polytropicStar(eos->getEos(), radius, mass, n);

    const Float rho_min = material->getParam<Interval>(BodySettingsId::DENSITY_RANGE).lower();
    Array<Float> m(points.size());
    Array<Float> rho(points.size());
    Array<Float> u(points.size());
    Array<Float> p(points.size());
    const Float v = domain.getVolume() / points.size();
    for (Size i = 0; i < points.size(); ++i) {
        const Float r = getLength(points[i]);
        rho[i] = max(star.rho(r), rho_min);
        u[i] = star.u(r);
        p[i] = star.p(r);
        m[i] = rho[i] * v;
    }

    /// \todo add to params
    const Float eta = material->getParam<Float>(BodySettingsId::SMOOTHING_LENGTH_ETA);
    for (Size i = 0; i < points.size(); ++i) {
        points[i][H] *= eta;
    }

    Storage storage(material);

    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(points));
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, std::move(m));
    storage.insert<Float>(QuantityId::ENERGY, OrderEnum::ZERO, std::move(u));
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, std::move(rho));
    storage.insert<Float>(QuantityId::PRESSURE, OrderEnum::ZERO, std::move(p));
    storage.insert<Float>(QuantityId::SOUND_SPEED, OrderEnum::ZERO, 100._f);
    storage.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, 0);

    MaterialInitialContext context;
    context.scheduler = scheduler;
    context.rng = makeRng<UniformRng>();
    material->create(storage, context);

    return storage;
}

NAMESPACE_SPH_END
