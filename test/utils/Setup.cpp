#include "utils/Setup.h"
#include "geometry/Domain.h"
#include "physics/Eos.h"
#include "physics/Rheology.h"
#include "sph/initial/Distribution.h"
#include "sph/solvers/GenericSolver.h"

NAMESPACE_SPH_BEGIN

namespace Tests {
    Storage getStorage(const Size particleCnt) {
        BodySettings settings;
        settings.set(BodySettingsId::DENSITY, 1._f);
        Storage storage(makeAuto<NullMaterial>(settings));
        HexagonalPacking distribution;
        SphericalDomain domain(Vector(0._f), 1._f);
        storage.insert<Vector>(
            QuantityId::POSITIONS, OrderEnum::SECOND, distribution.generate(particleCnt, domain));
        storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, 1._f);
        storage.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, 0);
        // density = 1, therefore total mass = volume, therefore mass per particle = volume / N
        storage.insert<Float>(
            QuantityId::MASSES, OrderEnum::ZERO, sphereVolume(1._f) / storage.getParticleCnt());
        return storage;
    }

    Storage getGassStorage(const Size particleCnt,
        BodySettings settings,
        const Float radius,
        const Float rho0,
        const Float u0) {
        settings.set(BodySettingsId::EOS, EosEnum::IDEAL_GAS)
            .set(BodySettingsId::ENERGY, u0)
            .set(BodySettingsId::DENSITY, rho0)
            .set(BodySettingsId::DENSITY_RANGE, Range(1.e-3_f * rho0, INFTY))
            .set(BodySettingsId::RHEOLOGY_DAMAGE, DamageEnum::NONE)
            .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE);
        Storage storage(makeAuto<EosMaterial>(settings, Factory::getEos(settings)));
        HexagonalPacking distribution;
        SphericalDomain domain(Vector(0._f), radius);
        storage.insert<Vector>(
            QuantityId::POSITIONS, OrderEnum::SECOND, distribution.generate(particleCnt, domain));
        storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, rho0);
        const Float m0 = rho0 * sphereVolume(radius) / storage.getParticleCnt();
        storage.insert<Float>(QuantityId::MASSES, OrderEnum::ZERO, m0);
        storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, u0);
        MaterialView material = storage.getMaterial(0);
        material->create(storage, MaterialInitialContext());
        material->initialize(storage, material.sequence());
        return storage;
    }

    Storage getSolidStorage(const Size particleCnt,
        BodySettings settings,
        const Float radius,
        const Float rho0,
        const Float u0) {
        settings.set(BodySettingsId::EOS, EosEnum::TILLOTSON)
            .set(BodySettingsId::ENERGY, u0)
            .set(BodySettingsId::DENSITY, rho0)
            .set(BodySettingsId::DENSITY_RANGE, Range(1.e-3_f * rho0, INFTY));
        Storage storage(
            makeAuto<SolidMaterial>(settings, Factory::getEos(settings), Factory::getRheology(settings)));
        HexagonalPacking distribution;
        SphericalDomain domain(Vector(0._f), radius);
        storage.insert<Vector>(
            QuantityId::POSITIONS, OrderEnum::SECOND, distribution.generate(particleCnt, domain));
        storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, rho0);
        const Float m0 = rho0 * sphereVolume(radius) / storage.getParticleCnt();
        storage.insert<Float>(QuantityId::MASSES, OrderEnum::ZERO, m0);
        storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, u0);
        storage.insert<TracelessTensor>(
            QuantityId::DEVIATORIC_STRESS, OrderEnum::FIRST, TracelessTensor::null());
        storage.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, 0);
        MaterialView material = storage.getMaterial(0);
        material->create(storage, MaterialInitialContext());
        material->initialize(storage, material.sequence());
        return storage;
    }
}

NAMESPACE_SPH_END
