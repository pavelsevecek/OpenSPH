#include "sph/solvers/StandardSets.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

EquationHolder getStandardEquations(const RunSettings& settings, const EquationHolder& other) {
    ASSERT(settings.get<FormulationEnum>(RunSettingsId::SPH_FORMULATION) == FormulationEnum::STANDARD);
    EquationHolder equations;
    /// \todo test that all possible combination (pressure, stress, AV, ...) work and dont assert
    if (settings.get<bool>(RunSettingsId::MODEL_FORCE_PRESSURE_GRADIENT)) {
        equations += makeTerm<StandardPressureForce>();
    }
    if (settings.get<bool>(RunSettingsId::MODEL_FORCE_NAVIER_STOKES)) {
        equations += makeTerm<NavierStokesForce>();
    } else if (settings.get<bool>(RunSettingsId::MODEL_FORCE_SOLID_STRESS)) {
        equations += makeTerm<StandardSolidStressForce>(settings);
    }
    equations += makeTerm<StandardContinuityEquation>();

    // artificial viscosity
    equations += EquationHolder(Factory::getArtificialViscosity(settings));

    // adaptivity of smoothing length
    Flags<SmoothingLengthEnum> hflags =
        settings.getFlags<SmoothingLengthEnum>(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH);
    if (hflags.has(SmoothingLengthEnum::CONTINUITY_EQUATION)) {
        equations += makeTerm<StandardAdaptiveSmoothingLength>(settings);
    } else {
        /// \todo add test checking that with ConstSmoothingLength the h will indeed be const
        equations += makeTerm<ConstSmoothingLength>();
    }

    equations += other;

    return equations;
}

NAMESPACE_SPH_END
