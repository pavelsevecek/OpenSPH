#include "tests/Setup.h"
#include "objects/geometry/Domain.h"
#include "physics/Eos.h"
#include "physics/Rheology.h"
#include "quantities/Quantity.h"
#include "sph/Materials.h"
#include "sph/initial/Distribution.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

Storage Tests::getStorage(const Size particleCnt) {
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, 1._f);
    Storage storage(makeAuto<NullMaterial>(settings));

    AutoPtr<IDistribution> distribution = Factory::getDistribution(settings);
    SphericalDomain domain(Vector(0._f), 1._f);
    Array<Vector> positions = distribution->generate(SEQUENTIAL, particleCnt, domain);
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(positions));

    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, 1._f);
    storage.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, 0);
    // density = 1, therefore total mass = volume, therefore mass per particle = volume / N
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, sphereVolume(1._f) / storage.getParticleCnt());
    return storage;
}

Storage Tests::getGassStorage(const Size particleCnt, BodySettings settings, const IDomain& domain) {
    // setup settings
    const Float rho0 = settings.get<Float>(BodySettingsId::DENSITY);
    const Float u0 = settings.get<Float>(BodySettingsId::ENERGY);
    settings.set(BodySettingsId::EOS, EosEnum::IDEAL_GAS)
        .set(BodySettingsId::DENSITY_RANGE, Interval(1.e-3_f * rho0, INFTY))
        .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE)
        .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE);

    // create storage and particle positions
    Storage storage(makeAuto<EosMaterial>(settings, Factory::getEos(settings)));
    AutoPtr<IDistribution> distribution = Factory::getDistribution(settings);
    Array<Vector> r = distribution->generate(SEQUENTIAL, particleCnt, domain);
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(r));

    // set needed quantities and materials
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, rho0);
    const Float m0 = rho0 * domain.getVolume() / storage.getParticleCnt();
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, m0);
    storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, u0);
    MaterialView material = storage.getMaterial(0);
    material->create(storage, MaterialInitialContext());
    material->initialize(SEQUENTIAL, storage, material.sequence());
    return storage;
}

Storage Tests::getGassStorage(const Size particleCnt, BodySettings settings, const Float radius) {
    return getGassStorage(particleCnt, settings, SphericalDomain(Vector(0.f), radius));
}

Storage Tests::getSolidStorage(const Size particleCnt, BodySettings settings, const IDomain& domain) {
    const Float u0 = settings.get<Float>(BodySettingsId::ENERGY);
    const Float rho0 = settings.get<Float>(BodySettingsId::DENSITY);
    settings.set(BodySettingsId::EOS, EosEnum::TILLOTSON)
        .set(BodySettingsId::DENSITY_RANGE, Interval(1.e-3_f * rho0, INFTY));

    AutoPtr<IRheology> rheology = Factory::getRheology(settings);
    if (!rheology) {
        rheology = makeAuto<ElasticRheology>();
    }

    Storage storage(makeAuto<SolidMaterial>(settings, Factory::getEos(settings), std::move(rheology)));
    AutoPtr<IDistribution> distribution = Factory::getDistribution(settings);
    Array<Vector> positions = distribution->generate(SEQUENTIAL, particleCnt, domain);
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(positions));

    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, rho0);
    const Float m0 = rho0 * domain.getVolume() / storage.getParticleCnt();
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, m0);
    storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, u0);
    storage.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS, OrderEnum::FIRST, TracelessTensor::null());
    storage.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, 0);
    MaterialView material = storage.getMaterial(0);

    MaterialInitialContext context;
    context.rng = makeRng<UniformRng>();
    material->create(storage, context);
    material->initialize(SEQUENTIAL, storage, material.sequence());
    return storage;
}

Storage Tests::getSolidStorage(const Size particleCnt, BodySettings settings, const Float radius) {
    return getSolidStorage(particleCnt, settings, SphericalDomain(Vector(0.f), radius));
}

Size Tests::getClosestParticle(const Storage& storage, const Vector& p) {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    Float closestDistSqr = INFTY;
    Size closestIdx = Size(-1);
    for (Size i = 0; i < r.size(); ++i) {
        const Float distSqr = getSqrLength(r[i] - p);
        if (distSqr < closestDistSqr) {
            closestDistSqr = distSqr;
            closestIdx = i;
        }
    }
    ASSERT(closestDistSqr < INFTY);
    return closestIdx;
}


NAMESPACE_SPH_END
