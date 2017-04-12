#include "sph/Material.h"
#include "physics/Eos.h"
#include "physics/Rheology.h"
#include "quantities/Storage.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

EosMaterial::EosMaterial(std::unique_ptr<Abstract::Eos>&& eos)
    : eos(std::move(eos)) {}

void EosMaterial::initialize(Storage& storage, const MaterialSequence sequence) {
    tie(rho, u, p, cs) = storage.getValues<Float>(
        QuantityIds::DENSITY, QuantityIds::ENERGY, QuantityIds::PRESSURE, QuantityIds::SOUND_SPEED);
    for (Size i : sequence) {
        /// \todo now we can easily pass sequence into the EoS and iterate inside, to avoid calling
        /// virtual function (and we could also optimize with SSE)
        tie(p[i], cs[i]) = eos->evaluate(rho[i], u[i]);
    }
}

SolidMaterial::SolidMaterial(std::unique_ptr<Abstract::Eos>&& eos,
    std::unique_ptr<Abstract::Rheology>&& rheology)
    : EosMaterial(std::move(eos))
    , rheology(std::move(rheology)) {}

void SolidMaterial::initialize(Storage& storage, const MaterialSequence sequence) {
    EosMaterial::initialize(storage, sequence);
    rheology->initialize(storage, sequence);
}

void SolidMaterial::finalize(Storage& storage, const MaterialSequence sequence) {
    rheology->integrate(storage, sequence);
}

std::unique_ptr<Abstract::Material> getDefaultMaterial() {
    return Factory::getMaterial(BodySettings::getDefaults());
}

NAMESPACE_SPH_END
