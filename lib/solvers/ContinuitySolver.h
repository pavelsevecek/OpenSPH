#pragma once

/// Standard SPH solver, using density and specific energy as independent variables. Evolves density using
/// continuity equation and energy using energy equation. Works with any artificial viscosity and any equation
/// of state.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"
#include "solvers/AbstractSolver.h"
#include "sph/av/Standard.h"
#include "sph/boundary/Boundary.h"
#include "sph/kernel/Kernel.h"
#include "storage/QuantityKey.h"
#include "system/Settings.h"


NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Finder;
    class Eos;
}
struct NeighbourRecord;



template <int d, typename TForce>
class ContinuitySolver : public SolverBase<d> {
private:
    TForce force;

public:
    ContinuitySolver(const Settings<GlobalSettingsIds>& settings)
        : SolverBase<d>(settings)
        , force(settings) {

        /// \todo we have to somehow connect EoS with storage. Plus when two storages are merged, we have to
        /// remember EoS for each particles. This should be probably generalized, for example we want to
        /// remember
        /// original body of the particle, possibly original position (right under surface, core of the body,
        /// antipode, ...)

        /// \todo !!!toto je ono, tady nejde globalni nastaveni
        eos = Factory::getEos(BODY_SETTINGS);

        std::unique_ptr<Abstract::Domain> domain = Factory::getDomain(settings);
        this->boundary = Factory::getBoundaryConditions(settings, storage, std::move(domain));
    }

    virtual void compute(Storage& storage) override;
};

NAMESPACE_SPH_END
