#include "sph/Materials.h"
#include "io/Logger.h"
#include "physics/Eos.h"
#include "physics/Rheology.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "system/Factory.h"
#include "thread/CheckFunction.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

EosMaterial::EosMaterial(const BodySettings& body, AutoPtr<IEos>&& eos)
    : IMaterial(body)
    , eos(std::move(eos)) {
    SPH_ASSERT(this->eos);
}

EosMaterial::EosMaterial(const BodySettings& body)
    : EosMaterial(body, Factory::getEos(body)) {}

const IEos& EosMaterial::getEos() const {
    return *eos;
}

void EosMaterial::create(Storage& storage, const MaterialInitialContext& UNUSED(context)) {
    VERBOSE_LOG
    SPH_ASSERT(storage.getMaterialCnt() == 1);

    // set to defaults if not yet created
    const Float rho0 = this->getParam<Float>(BodySettingsId::DENSITY);
    const Float u0 = this->getParam<Float>(BodySettingsId::ENERGY);
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, rho0);
    storage.insert<Float>(QuantityId::ENERGY, OrderEnum::ZERO, u0);

    ArrayView<const Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
    ArrayView<const Float> u = storage.getValue<Float>(QuantityId::ENERGY);
    const Size n = storage.getParticleCnt();
    Array<Float> p(n), cs(n);
    for (Size i = 0; i < n; ++i) {
        tie(p[i], cs[i]) = eos->evaluate(rho[i], u[i]);
    }
    storage.insert<Float>(QuantityId::PRESSURE, OrderEnum::ZERO, std::move(p));
    storage.insert<Float>(QuantityId::SOUND_SPEED, OrderEnum::ZERO, std::move(cs));
}

void EosMaterial::initialize(IScheduler& scheduler, Storage& storage, const IndexSequence sequence) {
    VERBOSE_LOG

    ArrayView<Float> rho, u, p, cs;
    tie(rho, u, p, cs) = storage.getValues<Float>(
        QuantityId::DENSITY, QuantityId::ENERGY, QuantityId::PRESSURE, QuantityId::SOUND_SPEED);
    parallelFor(scheduler, sequence, [&](const Size i) INL {
        /// \todo now we can easily pass sequence into the EoS and iterate inside, to avoid calling
        /// virtual function (and we could also optimize with SSE)
        tie(p[i], cs[i]) = eos->evaluate(rho[i], u[i]);
    });
}

SolidMaterial::SolidMaterial(const BodySettings& body, AutoPtr<IEos>&& eos, AutoPtr<IRheology>&& rheology)
    : EosMaterial(body, std::move(eos))
    , rheology(std::move(rheology)) {}

SolidMaterial::SolidMaterial(const BodySettings& body)
    : SolidMaterial(body, Factory::getEos(body), Factory::getRheology(body)) {}

void SolidMaterial::create(Storage& storage, const MaterialInitialContext& context) {
    VERBOSE_LOG

    EosMaterial::create(storage, context);
    rheology->create(storage, *this, context);
}

void SolidMaterial::initialize(IScheduler& scheduler, Storage& storage, const IndexSequence sequence) {
    VERBOSE_LOG

    EosMaterial::initialize(scheduler, storage, sequence);
    rheology->initialize(scheduler, storage, MaterialView(this, sequence));
}

void SolidMaterial::finalize(IScheduler& scheduler, Storage& storage, const IndexSequence sequence) {
    VERBOSE_LOG

    EosMaterial::finalize(scheduler, storage, sequence);
    rheology->integrate(scheduler, storage, MaterialView(this, sequence));
}

AutoPtr<IMaterial> getMaterial(const MaterialEnum type) {
    CHECK_FUNCTION(CheckFunction::NO_THROW);

    BodySettings settings;
    switch (type) {
    case MaterialEnum::BASALT:
        // this is the default, so we don't have to do anything
        break;
    case MaterialEnum::ICE:
        settings.set(BodySettingsId::TILLOTSON_SMALL_A, 0.3_f)
            .set(BodySettingsId::TILLOTSON_SMALL_B, 0.1_f)
            .set(BodySettingsId::TILLOTSON_SUBLIMATION, 1.e7_f)
            .set(BodySettingsId::DENSITY, 917._f)
            .set(BodySettingsId::BULK_MODULUS, 9.47e9_f)
            .set(BodySettingsId::TILLOTSON_NONLINEAR_B, 9.47e9_f)
            .set(BodySettingsId::TILLOTSON_ENERGY_IV, 7.73e5_f)
            .set(BodySettingsId::TILLOTSON_ENERGY_CV, 3.04e6_f)
            .set(BodySettingsId::TILLOTSON_ALPHA, 10._f)
            .set(BodySettingsId::TILLOTSON_BETA, 5._f);
        break;
    case MaterialEnum::IRON:
        settings.set(BodySettingsId::TILLOTSON_SMALL_A, 0.5_f)
            .set(BodySettingsId::TILLOTSON_SMALL_B, 1.5_f)
            .set(BodySettingsId::TILLOTSON_SUBLIMATION, 9.5e6_f)
            .set(BodySettingsId::DENSITY, 7860._f)
            .set(BodySettingsId::BULK_MODULUS, 1.28e11_f)
            .set(BodySettingsId::TILLOTSON_NONLINEAR_B, 1.05e11_f)
            .set(BodySettingsId::SHEAR_MODULUS, 8.2e10_f)
            .set(BodySettingsId::ELASTICITY_LIMIT, 3.5e8_f)
            .set(BodySettingsId::MELT_ENERGY, 1.e6_f)
            .set(BodySettingsId::TILLOTSON_ENERGY_IV, 1.42e6_f)
            .set(BodySettingsId::TILLOTSON_ENERGY_CV, 8.45e6_f)
            .set(BodySettingsId::TILLOTSON_ALPHA, 5._f)
            .set(BodySettingsId::TILLOTSON_BETA, 5._f)
            .set(BodySettingsId::HEAT_CAPACITY, 449._f)
            .set(BodySettingsId::WEIBULL_COEFFICIENT, 1.e23_f)
            .set(BodySettingsId::WEIBULL_EXPONENT, 8);
        break;
    case MaterialEnum::OLIVINE:
        settings.set(BodySettingsId::TILLOTSON_SMALL_A, 0.5_f)
            .set(BodySettingsId::TILLOTSON_SMALL_B, 1.4_f)
            .set(BodySettingsId::TILLOTSON_SUBLIMATION, 5.5e8_f)
            .set(BodySettingsId::DENSITY, 3500._f)
            .set(BodySettingsId::BULK_MODULUS, 1.31e11_f)
            .set(BodySettingsId::TILLOTSON_NONLINEAR_B, 4.9e10_f)
            .set(BodySettingsId::TILLOTSON_ENERGY_IV, 4.5e6_f)
            .set(BodySettingsId::TILLOTSON_ENERGY_CV, 1.5e7_f)
            .set(BodySettingsId::TILLOTSON_ALPHA, 5._f)
            .set(BodySettingsId::TILLOTSON_BETA, 5._f);
        break;
    default:
        NOT_IMPLEMENTED;
    }

    return Factory::getMaterial(settings);
}

NAMESPACE_SPH_END
