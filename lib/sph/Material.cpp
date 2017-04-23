#include "sph/Material.h"
#include "physics/Eos.h"
#include "physics/Rheology.h"
#include "quantities/Storage.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

NullMaterial::NullMaterial(const BodySettings& body)
    : Abstract::Material(body) {}

EosMaterial::EosMaterial(const BodySettings& body, std::unique_ptr<Abstract::Eos>&& eos)
    : Abstract::Material(body)
    , eos(std::move(eos)) {
    ASSERT(this->eos);
}

Pair<Float> EosMaterial::evaluate(const Float rho, const Float u) const {
    return eos->evaluate(rho, u);
}

void EosMaterial::create(Storage& storage) const {
    const Float rho0 = this->getParam<Float>(BodySettingsId::DENSITY);
    const Float u0 = this->getParam<Float>(BodySettingsId::ENERGY);
    const Size n = storage.getParticleCnt();
    Array<Float> p(n), cs(n);
    for (Size i = 0; i < n; ++i) {
        tie(p[i], cs[i]) = eos->evaluate(rho0, u0);
    }
    storage.insert<Float>(QuantityId::PRESSURE, OrderEnum::ZERO, std::move(p));
    storage.insert<Float>(QuantityId::SOUND_SPEED, OrderEnum::ZERO, std::move(cs));
}

void EosMaterial::initialize(Storage& storage, const MaterialSequence sequence) {
    tie(rho, u, p, cs) = storage.getValues<Float>(
        QuantityId::DENSITY, QuantityId::ENERGY, QuantityId::PRESSURE, QuantityId::SOUND_SPEED);
    for (Size i : sequence) {
        /// \todo now we can easily pass sequence into the EoS and iterate inside, to avoid calling
        /// virtual function (and we could also optimize with SSE)
        tie(p[i], cs[i]) = eos->evaluate(rho[i], u[i]);
    }
}

SolidMaterial::SolidMaterial(const BodySettings& body,
    std::unique_ptr<Abstract::Eos>&& eos,
    std::unique_ptr<Abstract::Rheology>&& rheology)
    : EosMaterial(body, std::move(eos))
    , rheology(std::move(rheology)) {}

void SolidMaterial::create(Storage& storage) const {
    EosMaterial::create(storage);
    rheology->create(storage, params);
}

void SolidMaterial::initialize(Storage& storage, const MaterialSequence sequence) {
    EosMaterial::initialize(storage, sequence);
    rheology->initialize(storage, MaterialView(this, sequence, sequence.getId()));
}

void SolidMaterial::finalize(Storage& storage, const MaterialSequence sequence) {
    EosMaterial::finalize(storage, sequence);
    rheology->integrate(storage, MaterialView(this, sequence, sequence.getId()));
}

std::unique_ptr<Abstract::Material> getDefaultMaterial() {
    return Factory::getMaterial(BodySettings::getDefaults());
}

NAMESPACE_SPH_END
