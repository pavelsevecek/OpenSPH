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

template <typename TComposite>
class CompositeModel : public Abstract::Model {
private:
    std::unique_ptr<Abstract::Finder> finder;
    std::unique_ptr<Abstract::Eos> eos;

    LutKernel kernel;
    Array<NeighbourRecord> neighs; /// \todo store neighbours directly here?!

    Array<Float> divv; /// auxiliary buffer storing velocity divergences

    TComposite::Force force;   /// force getter
    TComposite::AV av;         /// artificial viscosity
    TComposite::KernelSym sym; /// kernel symmetrization


public:
    BasicModel(const std::shared_ptr<Storage>& storage, const Settings<GlobalSettingsIds>& settings);

    ~BasicModel(); // necessary because of unique_ptrs to incomplete types

    virtual void compute(Storage& storage) override;

    virtual Storage createParticles(const int n,
                                    std::unique_ptr<Abstract::Domain> domain,
                                    const Settings<BodySettingsIds>& settings) const override;
};

template <typename... TForces>
struct CompositeForce {
private:
    Tuple<TForces...> forces;

public:
    CompositeForce(TForces&&... forces)
        : forces(makeTuple(std::forward<TForces>(forces)...)) {}

    CompositeForce(Tuple<TForces...>&& forces)
        : forces(std::move(forces)) {}

    /// returns a force between two particles with indices i and j
    INLINE Vector operator()(const int i, const int j, const Vector& grad) {
        Vector result(0._f);
        forEach(forces, [i, j, &grad, &result](auto&& f) { result += f(i, j, grad); });
    }

    template <typename TOther>
    CompositeForce<TForces..., TOther> add(TOther&& other) const {
        return CompositeForces<TForces..., TOther>(append(forces, other));
    }
};




NAMESPACE_SPH_END
