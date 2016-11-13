#include "models/BasicModel.h"
#include "storage/Iterate.h"
#include "system/Factory.h"
#include "objects/finders/Finder.h"
#include "physics/Eos.h"
#include "sph/initconds/InitConds.h"

NAMESPACE_SPH_BEGIN

BasicModel::BasicModel(const std::shared_ptr<Storage>& storage, const Settings<GlobalSettingsIds>& settings)
    : Abstract::Model(storage) {
    finder = Factory::getFinder((FinderEnum)settings.get<int>(GlobalSettingsIds::FINDER).get());
    kernel = Factory::getKernel((KernelEnum)settings.get<int>(GlobalSettingsIds::KERNEL).get());
}

BasicModel::~BasicModel() = default;

Storage BasicModel::createParticles(const int n,
                                    std::unique_ptr<Abstract::Domain> domain,
                                    const Settings<BodySettingsIds>& settings) const {
    std::unique_ptr<Abstract::Distribution> distribution = Factory::getDistribution(settings);

    // generate positions of particles
    Array<Vector> rs = distribution->generate(n, domain.get());

    // final number of particles
    const int N = rs.size();

    Storage st;

    // put generated particles inside the storage
    st.insert<QuantityKey::R>(std::move(rs));

    // create all other quantities (with empty arrays so far)
    st.insert<QuantityKey::M, QuantityKey::P, QuantityKey::RHO, QuantityKey::U>();

    // allocate all arrays
    // Note: derivatives of positions (velocity, accelerations) are set to 0 by inserting the array. All other
    // quantities are constant or 1st order, so their derivatives will be cleared by timestepping, we don't
    // have to do this here.
    iterate<TemporalEnum::ALL>(st, [N](auto&& array) { array.resize(N); });

    // set density to default value
    const Optional<Float> rho0 = settings.get<Float>(BodySettingsIds::DENSITY);
    ASSERT(rho0);
    st.get<QuantityKey::RHO>().fill(rho0.get());

    // set internal energy to default value
    const Optional<Float> u0 = settings.get<Float>(BodySettingsIds::ENERGY);
    ASSERT(u0);
    st.get<QuantityKey::U>().fill(u0.get());

    // compute pressure using equation of state
    Optional<EosEnum> eosId = optionalCast<EosEnum>(settings.get<int>(BodySettingsIds::EOS));
    ASSERT(eosId);
    std::unique_ptr<Abstract::Eos> eos = Factory::getEos(eosId.get(), settings);
    ArrayView<Float> rhos, us, ps;
    tie(rhos, us, ps) = st.get<QuantityKey::RHO, QuantityKey::U, QuantityKey::P>();
    eos->getPressure(rhos, us, ps);

    return st;
}


NAMESPACE_SPH_END
