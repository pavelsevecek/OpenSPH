#include "physics/Eos.h"
#include "physics/Rheology.h"
#include "quantities/Storage.h"
#include "sph/Materials.h"
#include "system/Factory.h"
#include "thread/Pool.h"

NAMESPACE_SPH_BEGIN

EosMaterial::EosMaterial(const BodySettings& body, AutoPtr<IEos>&& eos)
    : IMaterial(body)
    , eos(std::move(eos)) {
    ASSERT(this->eos);
}

EosMaterial::EosMaterial(const BodySettings& body)
    : EosMaterial(body, Factory::getEos(body)) {}

Pair<Float> EosMaterial::evaluate(const Float rho, const Float u) const {
    return eos->evaluate(rho, u);
}

const IEos& EosMaterial::getEos() const {
    return *eos;
}

void EosMaterial::create(Storage& storage, const MaterialInitialContext& UNUSED(context)) {
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

void EosMaterial::initialize(Storage& storage, const IndexSequence sequence) {
    tie(rho, u, p, cs) = storage.getValues<Float>(
        QuantityId::DENSITY, QuantityId::ENERGY, QuantityId::PRESSURE, QuantityId::SOUND_SPEED);
    parallelFor(*sequence.begin(), *sequence.end(), [&](const Size n1, const Size n2) INL {
        /// \todo now we can easily pass sequence into the EoS and iterate inside, to avoid calling
        /// virtual function (and we could also optimize with SSE)
        for (Size i = n1; i < n2; ++i) {
            tie(p[i], cs[i]) = eos->evaluate(rho[i], u[i]);
        }
    });
}

SolidMaterial::SolidMaterial(const BodySettings& body, AutoPtr<IEos>&& eos, AutoPtr<IRheology>&& rheology)
    : EosMaterial(body, std::move(eos))
    , rheology(std::move(rheology)) {}

SolidMaterial::SolidMaterial(const BodySettings& body)
    : SolidMaterial(body, Factory::getEos(body), Factory::getRheology(body)) {}

void SolidMaterial::create(Storage& storage, const MaterialInitialContext& context) {
    EosMaterial::create(storage, context);
    rheology->create(storage, *this, context);
}

void SolidMaterial::initialize(Storage& storage, const IndexSequence sequence) {
    EosMaterial::initialize(storage, sequence);
    rheology->initialize(storage, MaterialView(this, sequence));
}

void SolidMaterial::finalize(Storage& storage, const IndexSequence sequence) {
    EosMaterial::finalize(storage, sequence);
    rheology->integrate(storage, MaterialView(this, sequence));
}

AutoPtr<IMaterial> getDefaultMaterial() {
    return Factory::getMaterial(BodySettings::getDefaults());
}

NAMESPACE_SPH_END
