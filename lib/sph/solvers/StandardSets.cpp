#include "sph/solvers/StandardSets.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

EquationHolder getStandardEquations(const RunSettings& settings, const EquationHolder& other) {
    ASSERT(settings.get<FormulationEnum>(RunSettingsId::SPH_FORMULATION) == FormulationEnum::STANDARD);
    EquationHolder equations;
    /// \todo test that all possible combination (pressure, stress, AV, ...) work and dont assert
    if (settings.get<bool>(RunSettingsId::MODEL_FORCE_PRESSURE_GRADIENT)) {
        equations += makeTerm<StandardSph::PressureForce>();
    }
    if (settings.get<bool>(RunSettingsId::MODEL_FORCE_NAVIER_STOKES)) {
        equations += makeTerm<StandardSph::NavierStokesForce>();
    } else if (settings.get<bool>(RunSettingsId::MODEL_FORCE_SOLID_STRESS)) {
        equations += makeTerm<StandardSph::SolidStressForce>(settings);
    }
    equations += makeTerm<StandardSph::ContinuityEquation>();

    // artificial viscosity
    equations += EquationHolder(Factory::getArtificialViscosity(settings));

    // add all the additional equations
    equations += other;

    // adaptivity of smoothing length should be last as it sets 4th component of velocities (and
    // accelerations), this should be improved (similarly to derivatives - equation phases)
    Flags<SmoothingLengthEnum> hflags =
        settings.getFlags<SmoothingLengthEnum>(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH);
    if (hflags.has(SmoothingLengthEnum::CONTINUITY_EQUATION)) {
        equations += makeTerm<StandardSph::AdaptiveSmoothingLength>(settings);
    } else {
        /// \todo add test checking that with ConstSmoothingLength the h will indeed be const
        equations += makeTerm<ConstSmoothingLength>();
    }

    return equations;
}

EquationHolder getBenzAsphaugEquations(const RunSettings& settings, const EquationHolder& other) {
    ASSERT(settings.get<FormulationEnum>(RunSettingsId::SPH_FORMULATION) == FormulationEnum::BENZ_ASPHAUG);
    EquationHolder equations;
    if (settings.get<bool>(RunSettingsId::MODEL_FORCE_PRESSURE_GRADIENT)) {
        equations += makeTerm<BenzAsphaugSph::PressureForce>();
    }
    if (settings.get<bool>(RunSettingsId::MODEL_FORCE_NAVIER_STOKES)) {
        NOT_IMPLEMENTED;
    } else if (settings.get<bool>(RunSettingsId::MODEL_FORCE_SOLID_STRESS)) {
        equations += makeTerm<BenzAsphaugSph::SolidStressForce>(settings);
    }
    equations += makeTerm<BenzAsphaugSph::ContinuityEquation>();

    // artificial viscosity
    equations += EquationHolder(Factory::getArtificialViscosity(settings));

    // add all the additional equations
    equations += other;

    // adaptivity of smoothing length should be last as it sets 4th component of velocities (and
    // accelerations), this should be improved (similarly to derivatives - equation phases)
    Flags<SmoothingLengthEnum> hflags =
        settings.getFlags<SmoothingLengthEnum>(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH);
    if (hflags.has(SmoothingLengthEnum::CONTINUITY_EQUATION)) {
        equations += makeTerm<BenzAsphaugSph::AdaptiveSmoothingLength>(settings);
    } else {
        /// \todo add test checking that with ConstSmoothingLength the h will indeed be const
        equations += makeTerm<ConstSmoothingLength>();
    }

    return equations;
}


NAMESPACE_SPH_END
