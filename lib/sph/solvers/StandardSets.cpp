#include "sph/solvers/StandardSets.h"
#include "sph/equations/Friction.h"
#include "sph/equations/HelperTerms.h"
#include "sph/equations/Potentials.h"
#include "sph/equations/av/Stress.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

EquationHolder getStandardEquations(const RunSettings& settings, const EquationHolder& other) {
    EquationHolder equations;
    /// \todo test that all possible combination (pressure, stress, AV, ...) work and dont assert
    Flags<ForceEnum> forces = settings.getFlags<ForceEnum>(RunSettingsId::SOLVER_FORCES);
    if (forces.has(ForceEnum::PRESSURE)) {
        equations += makeTerm<PressureForce>();

        if (forces.has(ForceEnum::NAVIER_STOKES)) {
            equations += makeTerm<NavierStokesForce>();
        } else if (forces.has(ForceEnum::SOLID_STRESS)) {
            equations += makeTerm<SolidStressForce>(settings);
        }
    }

    if (forces.has(ForceEnum::INTERNAL_FRICTION)) {
        /// \todo this term (and also AV) do not depend on particular equation set, so it could be moved
        /// outside to reduce code duplication, but this also provides a way to get all necessary terms by
        /// calling a single function ...
        equations += makeTerm<ViscousStress>();
    }

    if (forces.has(ForceEnum::INERTIAL)) {
        const Vector omega = settings.get<Vector>(RunSettingsId::FRAME_ANGULAR_FREQUENCY);
        equations += makeTerm<InertialForce>(omega);
    }

    equations += makeTerm<ContinuityEquation>();

    // artificial viscosity
    equations += EquationHolder(Factory::getArtificialViscosity(settings));

    // equations += makeTerm<EffectiveNeighbourCountTerm>();
    // equations += makeTerm<StressAV>(settings);

    // add all the additional equations
    equations += other;

    // adaptivity of smoothing length should be last as it sets 4th component of velocities (and
    // accelerations), this should be improved (similarly to derivatives - equation phases)
    Flags<SmoothingLengthEnum> hflags =
        settings.getFlags<SmoothingLengthEnum>(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH);
    if (hflags.has(SmoothingLengthEnum::CONTINUITY_EQUATION)) {
        equations += makeTerm<AdaptiveSmoothingLength>(settings);
    } else {
        /// \todo add test checking that with ConstSmoothingLength the h will indeed be const
        equations += makeTerm<ConstSmoothingLength>();
    }

    return equations;
}

NAMESPACE_SPH_END
