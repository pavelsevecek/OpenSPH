#include "storage/BasicView.h"
#include "sph/initconds/InitConds.h"

NAMESPACE_SPH_BEGIN

/*void BasicView::create(const int n,
                       const Abstract::Domain* domain,
                       const Abstract::Distribution* distribution,
                       const Settings<BodySettingsIds>& settings) {
    rs = distribution->generate(n, domain);
    N  = rs.size();
    // storage.iterate<StorageIterableType::ALL>([this](auto&& array) { array.resize(N); });

    Optional<EosEnum> eosId = optionalCast<EosEnum>(settings.get<int>(BodySettingsIds::EOS));
    ASSERT(eosId);
    std::unique_ptr<Abstract::Eos> eos = Factory::getEos(eosId.get(), settings);


    vs.fill(Vector(0._f));
    const Optional<Float> rho = settings.get<Float>(BodySettingsIds::DENSITY);
    rhos.fill(rho.get());
    us.fill(0._f);

    // get initial pressure
    eos->getPressure(rhos, us, ps);
}*/

NAMESPACE_SPH_END
