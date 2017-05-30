#pragma once

/// \file ContinuitySolver.h
/// \brief Standard SPH formulation that uses continuity equation for solution of density
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "sph/equations/av/Standard.h"
#include "sph/solvers/GenericSolver.h"

NAMESPACE_SPH_BEGIN


/// \brief Standard SPH solver using density and specific energy as independent variables.
///
/// Evolves density using continuity equation and energy using energy equation. Works with any artificial
/// viscosity and any equation of state.
class ContinuitySolver : public GenericSolver {
public:
    ContinuitySolver(const RunSettings& settings,
        const EquationHolder& additionalEquations = EquationHolder())
        : GenericSolver(settings, this->getEquations(settings, additionalEquations)) {}

private:
    EquationHolder getEquations(const RunSettings& settings, const EquationHolder& additionalEquations) {
        EquationHolder equations;
        /// \todo test that all possible combination (pressure, stress, AV, ...) work and dont assert
        if (settings.get<bool>(RunSettingsId::MODEL_FORCE_PRESSURE_GRADIENT)) {
            equations += makeTerm<PressureForce>();
        }
        if (settings.get<bool>(RunSettingsId::MODEL_FORCE_SOLID_STRESS)) {
            equations += makeTerm<SolidStressForce>(settings);
        }
        equations += makeTerm<ContinuityEquation>(settings);

        // artificial viscosity
        equations += EquationHolder(Factory::getArtificialViscosity(settings));

        // adaptivity of smoothing length
        if (settings.get<SmoothingLengthEnum>(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH) !=
            SmoothingLengthEnum::CONST) {
            equations += makeTerm<AdaptiveSmoothingLength>(settings);
        }

        equations += additionalEquations;

        return equations;
    }
};

NAMESPACE_SPH_END
