#pragma once

/// Definition of physical model.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "models/AbstractModel.h"
#include "objects/Object.h"
#include "sph/kernel/Kernel.h"
#include "storage/QuantityMap.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Finder;
    class Eos;
}
struct NeighbourRecord;

class BasicModel : public Abstract::Model {
private:
    std::unique_ptr<Abstract::Finder> finder;
     std::unique_ptr<Abstract::Eos> eos;
    /// \todo what if we have more EoSs in one model? (rock and ice together in a comet)
    LutKernel kernel;
    Array<NeighbourRecord> neighs; /// \todo store neighbours directly here?!

    Array<Float> divv; /// auxiliary buffer storing velocity divergences

public:
    BasicModel(const std::shared_ptr<Storage>& storage, const Settings<GlobalSettingsIds>& settings);

    ~BasicModel(); // necessary because of unique_ptrs to incomplete types

    virtual void compute(Storage& storage) override;

    virtual Storage createParticles(const int n,
                                    std::unique_ptr<Abstract::Domain> domain,
                                    const Settings<BodySettingsIds>& settings) const override;

private:
    void solveSmoothingLength(ArrayView<Vector> v, ArrayView<const Vector> r);

    void solveDensity(ArrayView<Float> drho, ArrayView<const Float> rho);

    void solveEnergy(ArrayView<Float> du, ArrayView<const Float> p, ArrayView<const Float> rho);
};

NAMESPACE_SPH_END
