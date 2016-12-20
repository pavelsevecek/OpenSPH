#include "solvers/SolverFactory.h"
#include "solvers/SummationSolver.h"
#include "solvers/ContinuitySolver.h"
#include "sph/forces/Factory.h"

NAMESPACE_SPH_BEGIN

template <int D>
class ForceVisitor : public Object {
public:
    template <typename Force>
    std::unique_ptr<Abstract::Solver> visit(const GlobalSettings& settings) const {
        const SolverEnum id = settings.get<SolverEnum>(GlobalSettingsIds::SOLVER_TYPE);
        switch (id) {
        case SolverEnum::CONTINUITY_SOLVER:
            return std::make_unique<ContinuitySolver<Force, D>>(settings);
        case SolverEnum::SUMMATION_SOLVER:
            return std::make_unique<SummationSolver<Force, D>>(settings);
        default:
            NOT_IMPLEMENTED;
        }
    }
};

std::unique_ptr<Abstract::Solver> getSolver(const GlobalSettings& settings) {
    const int dim = settings.get<int>(GlobalSettingsIds::SOLVER_DIMENSIONS);
    switch (dim) {
    case 1:
        return dispatchStressForce(settings, ForceVisitor<1>());
    case 2:
        return dispatchStressForce(settings, ForceVisitor<2>());
    case 3:
        return dispatchStressForce(settings, ForceVisitor<3>());
    default:
        NOT_IMPLEMENTED;
    }
}

NAMESPACE_SPH_END
