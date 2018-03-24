#include "sph/solvers/StandardSets.h"
#include "sph/equations/Friction.h"
#include "sph/equations/HelperTerms.h"
#include "sph/equations/Potentials.h"
#include "sph/equations/av/Stress.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

EquationHolder getStandardEquations(const RunSettings& settings, const EquationHolder& other) {
    ASSERT(settings.get<FormulationEnum>(RunSettingsId::SPH_FORMULATION) == FormulationEnum::STANDARD);
    EquationHolder equations;
    /// \todo test that all possible combination (pressure, stress, AV, ...) work and dont assert
    Flags<ForceEnum> forces = settings.getFlags<ForceEnum>(RunSettingsId::SOLVER_FORCES);
    if (forces.has(ForceEnum::PRESSURE_GRADIENT)) {
        equations += makeTerm<StandardSph::PressureForce>();

        if (forces.has(ForceEnum::NAVIER_STOKES)) {
            equations += makeTerm<StandardSph::NavierStokesForce>();
        } else if (forces.has(ForceEnum::SOLID_STRESS)) {
            equations += makeTerm<StandardSph::SolidStressForce>(settings);
        }
    }

    if (forces.has(ForceEnum::INTERNAL_FRICTION)) {
        /// \todo this term (and also AV) do not depend on particular equation set, so it could be moved
        /// outside to reduce code duplication, but this also provides a way to get all necessary terms by
        /// calling a single function ...
        equations += makeTerm<StandardSph::ViscousStress>();
    }

    if (forces.has(ForceEnum::INERTIAL)) {
        const Vector omega = settings.get<Vector>(RunSettingsId::FRAME_ANGULAR_FREQUENCY);
        equations += makeTerm<InertialForce>(omega);
    }

    equations += makeTerm<StandardSph::ContinuityEquation>();

    // artificial viscosity
    equations += EquationHolder(Factory::getArtificialViscosity(settings));

    // equations += makeTerm<EffectiveNeighbourCountTerm>();
    equations += makeTerm<StressAV>(settings);

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
    Flags<ForceEnum> forces = settings.getFlags<ForceEnum>(RunSettingsId::SOLVER_FORCES);
    if (forces.has(ForceEnum::PRESSURE_GRADIENT)) {
        equations += makeTerm<BenzAsphaugSph::PressureForce>();

        if (forces.has(ForceEnum::NAVIER_STOKES)) {
            NOT_IMPLEMENTED;
        } else if (forces.has(ForceEnum::SOLID_STRESS)) {
            equations += makeTerm<BenzAsphaugSph::SolidStressForce>(settings);
        }
    }

    if (forces.has(ForceEnum::INTERNAL_FRICTION)) {
        NOT_IMPLEMENTED;
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

EquationHolder getEquations(const RunSettings& settings, const EquationHolder& other) {
    const FormulationEnum formulation = settings.get<FormulationEnum>(RunSettingsId::SPH_FORMULATION);
    switch (formulation) {
    case FormulationEnum::STANDARD:
        return getStandardEquations(settings, other);
    case FormulationEnum::BENZ_ASPHAUG:
        return getBenzAsphaugEquations(settings, other);
    default:
        NOT_IMPLEMENTED;
    }
}

NAMESPACE_SPH_END
