#include "utils/Setup.h"
#include "geometry/Domain.h"
#include "physics/Eos.h"
#include "solvers/GenericSolver.h"
#include "sph/initial/Distribution.h"

NAMESPACE_SPH_BEGIN

namespace Tests {
    Storage getStorage(const Size particleCnt) {
        BodySettings settings;
        settings.set(BodySettingsId::DENSITY, 1._f);
        Storage storage(std::make_unique<NullMaterial>(settings));
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

    /// Returns a storage with ideal gas particles, having pressure, energy and sound speed.
    Storage getGassStorage(const Size particleCnt, BodySettings settings) {
        settings.set(BodySettingsId::EOS, EosEnum::IDEAL_GAS);
        settings.set(BodySettingsId::ENERGY, 1._f);
        Storage storage(std::make_unique<EosMaterial>(settings, Factory::getEos(settings)));
        HexagonalPacking distribution;
        SphericalDomain domain(Vector(0._f), 1._f);
        storage.insert<Vector>(
            QuantityId::POSITIONS, OrderEnum::SECOND, distribution.generate(particleCnt, domain));
        storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, 1._f);
        storage.insert<Float>(
            QuantityId::MASSES, OrderEnum::ZERO, sphereVolume(1._f) / storage.getParticleCnt());
        storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, 1._f);
        MaterialView material = storage.getMaterial(0);
        material->create(storage);
        material->initialize(storage, material.sequence());
        return storage;
    }
}

NAMESPACE_SPH_END
