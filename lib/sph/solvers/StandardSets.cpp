#include "sph/solvers/StandardSets.h"
#include "sph/equations/Fluids.h"
#include "sph/equations/Friction.h"
#include "sph/equations/HelperTerms.h"
#include "sph/equations/Potentials.h"
#include "sph/equations/av/Stress.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

EquationHolder getStandardEquations(const RunSettings& settings, const EquationHolder& other) {
    EquationHolder equations;
    /// \todo test that all possible combination (pressure, stress, AV, ...) work and dont assert
    Flags<ForceEnum> forces = settings.getFlags<ForceEnum>(RunSettingsId::SPH_SOLVER_FORCES);
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

    if (forces.has(ForceEnum::SURFACE_TENSION)) {
        equations += makeTerm<CohesionTerm>();
    }

    if (forces.has(ForceEnum::INERTIAL)) {
        const Vector omega = settings.get<Vector>(RunSettingsId::FRAME_ANGULAR_FREQUENCY);
        equations += makeTerm<InertialForce>(omega);
    }

    const Vector g = settings.get<Vector>(RunSettingsId::FRAME_CONSTANT_ACCELERATION);
    if (g != Vector(0._f)) {
        equations += makeExternalForce([g](const Vector& UNUSED(r), const Float UNUSED(t)) { return g; });
    }

    const Float M = settings.get<Float>(RunSettingsId::FRAME_TIDES_MASS);
    if (M != 0._f) {
        const Vector R = settings.get<Vector>(RunSettingsId::FRAME_TIDES_POSITION);
        const Vector dir = getNormalized(R);
        equations += makeExternalForce([M, R, dir](const Vector& r, const Float UNUSED(t)) {
            return Constants::gravity * M * dot(r, dir) * dir / pow<3>(getLength(R));
        });
    }

    if (settings.get<bool>(RunSettingsId::SPH_SCRIPT_ENABLE)) {
        const Path scriptPath(settings.get<std::string>(RunSettingsId::SPH_SCRIPT_FILE));
        equations += makeTerm<ChaiScriptTerm>(scriptPath);
    }

    equations += makeTerm<ContinuityEquation>(settings);

    // artificial viscosity
    equations += EquationHolder(Factory::getArtificialViscosity(settings));

    if (settings.get<bool>(RunSettingsId::SPH_AV_USE_STRESS)) {
        equations += makeTerm<StressAV>(settings);
    }

    // add all the additional equations
    equations += other;

    // adaptivity of smoothing length should be last as it sets 4th component of velocities (and
    // accelerations), this should be improved (similarly to derivatives - equation phases)
    Flags<SmoothingLengthEnum> hflags =
        settings.getFlags<SmoothingLengthEnum>(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH);
    if (hflags.has(SmoothingLengthEnum::CONTINUITY_EQUATION)) {
        equations += makeTerm<AdaptiveSmoothingLength>(settings);
    } else {
        /// \todo add test checking that with ConstSmoothingLength the h will indeed be const
        equations += makeTerm<ConstSmoothingLength>();
    }

    return equations;
}

NAMESPACE_SPH_END
