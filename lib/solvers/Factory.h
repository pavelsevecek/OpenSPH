#pragma once

#include "solvers/ContinuitySolver.h"
#include "solvers/SummationSolver.h"
#include "sph/av/Factory.h"

NAMESPACE_SPH_BEGIN

class AvVisitor : public Object {
public:
    template <typename AV>
    std::unique_ptr<Abstract::Solver> visit(const Settings<GlobalSettingsIds>& settings) {
        const SolverEnum id = settings.get<SolveEnum>(GlobalSettingsIds::SOLVER_TYPE);
        switch (id) {
        case SolverEnum::CONTINUITY_SOLVER:
            return std::make_unique<ContinuitySolver<AV>>(settings);
        case SolverEnum::SUMMATION_SOLVER:
            return std::make_unique<SummationSolver<AV>>(settings);
        default:
            NOT_IMPLEMENTED;
        }
    }
};

/// Executes TVisitor::visit<Solver>(), where Solver is solver selected in settings.
INLINE std::unique_ptr<Abstract::Solver> getSolver(const Settings<GlobalSettingsIds>& settings) {
    return dispatchAV(settings, AvVisitor());
}


NAMESPACE_SPH_END
