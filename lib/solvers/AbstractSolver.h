#pragma once

#include "objects/Object.h"
#include "sph/kernel/Kernel.h"
#include "storage/Storage.h"
#include "system/Settings.h"
#include <memory>

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Domain;
    class Finder;
    class Boundary;
}
struct NeighbourRecord;

namespace Abstract {
    class Solver : public Polymorphic {
    public:
        /// Creates the solver, must be called before compute().
        virtual void create(Storage& storage) = 0;

        /// Solver can store references to quantities (as ArrayView) internally for fast access. When
        /// particles are added or removed, update() must be called to get updated pointers and sizes.
        /// This is espetially important when adding particles as pointers are potentially invalidated!
        virtual void update(Storage& storage) = 0;

        /// Computes derivatives of all time-dependent quantities
        virtual void compute() = 0;
    };
}

template <int d>
class SolverBase : public Abstract::Solver {
protected:
    std::unique_ptr<Abstract::Finder> finder;

    Array<NeighbourRecord> neighs;

    std::unique_ptr<Abstract::Boundary> boundary;

    LutKernel<d> kernel;

public:
    SolverBase(const Settings<GlobalSettingsIds>& settings) {
        finder = Factory::getFinder(settings);
        kernel = Factory::getKernel<d>(settings);

        std::unique_ptr<Abstract::Domain> domain = Factory::getDomain(settings);
        boundary = Factory::getBoundaryConditions(settings, std::move(domain));
    }
};

NAMESPACE_SPH_END
