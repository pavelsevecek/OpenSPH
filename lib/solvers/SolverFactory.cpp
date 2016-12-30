#include "solvers/SolverFactory.h"
#include "solvers/ContinuitySolver.h"
#include "solvers/DensityIndependentSolver.h"
#include "solvers/SummationSolver.h"
#include "sph/forces/Factory.h"

NAMESPACE_SPH_BEGIN

class ForceVisitor : public Object {
public:
    template <typename Force>
    std::unique_ptr<Abstract::Solver> visit(const GlobalSettings& settings) const {
        const SolverEnum id = settings.get<SolverEnum>(GlobalSettingsIds::SOLVER_TYPE);
        switch (id) {
        case SolverEnum::CONTINUITY_SOLVER:
            return std::make_unique<ContinuitySolver<Force, DIMENSIONS>>(settings);
        case SolverEnum::SUMMATION_SOLVER:
            return std::make_unique<SummationSolver<Force, DIMENSIONS>>(settings);
        default:
            NOT_IMPLEMENTED;
        }
    }
};

std::unique_ptr<Abstract::Solver> getSolver(const GlobalSettings& settings) {
    const SolverEnum id = settings.get<SolverEnum>(GlobalSettingsIds::SOLVER_TYPE);
    switch (id) {
    case SolverEnum::DENSITY_INDEPENDENT:
        return std::make_unique<DensityIndependentSolver<DIMENSIONS>>(settings);
    default:
        return dispatchStressForce(settings, ForceVisitor());
    }
}

NAMESPACE_SPH_END
