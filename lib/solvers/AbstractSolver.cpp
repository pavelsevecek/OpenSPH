#include "solvers/AbstractSolver.h"
#include "system/Factory.h"
#include "objects/finders/AbstractFinder.h"
#include "sph/boundary/Boundary.h"
#include "geometry/Domain.h"

NAMESPACE_SPH_BEGIN

template<Size D>
SolverBase<D>::SolverBase(const GlobalSettings& settings) {
    finder = Factory::getFinder(settings);
    kernel = Factory::getKernel<D>(settings);

    std::unique_ptr<Abstract::Domain> domain = Factory::getDomain(settings);
    boundary = Factory::getBoundaryConditions(settings, std::move(domain));
}

template<Size D>
SolverBase<D>::~SolverBase() = default;

template class SolverBase<1>;
template class SolverBase<2>;
template class SolverBase<3>;



NAMESPACE_SPH_END
