#pragma once

/// Definition of physical model.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "solvers/AbstractSolver.h"
#include "objects/Object.h"
#include "sph/av/Monaghan.h"
#include "sph/boundary/Boundary.h"
#include "sph/kernel/Kernel.h"
#include "storage/QuantityMap.h"
#include "system/Settings.h"


NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Finder;
    class Eos;
}
struct NeighbourRecord;

template <int d>
class SummationSolver : public Abstract::Solver {
private:
    std::unique_ptr<Abstract::Finder> finder;

    std::unique_ptr<Abstract::Eos> eos;

    std::unique_ptr<Abstract::BoundaryConditions> boundary;

    /// \todo what if we have more EoSs in one model? (rock and ice together in a comet)
    LutKernel<d> kernel;
    Array<NeighbourRecord> neighs; /// \todo store neighbours directly here?!


    Array<Float> accumulatedRho;
    Array<Float> accumulatedH;

    Float eta;

    MonaghanAV monaghanAv;



public:
    SummationSolver(const std::shared_ptr<Storage>& storage, const Settings<GlobalSettingsIds>& settings);

    ~SummationSolver(); // necessary because of unique_ptrs to incomplete types

    virtual void compute(Storage& storage) override;

    virtual Storage createParticles(const Abstract::Domain& domain,
                                    const Settings<BodySettingsIds>& settings) const override;

};

NAMESPACE_SPH_END
