#pragma once

/// \file Present.h
/// \brief Problem-specific initial conditions
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "io/Path.h"
#include "objects/wrappers/Function.h"
#include "sph/initial/Initial.h"

NAMESPACE_SPH_BEGIN

namespace Presets {

struct CollisionParams {
    /// Radius of the parent body in meters
    Float targetRadius;

    /// Radius of the projectile in meters
    Float impactorRadius;

    /// Impact speed in m/s
    Float impactSpeed;

    /// Impact angle, i.e. angle between velocity vector and normal at the impact point
    Float impactAngle;

    /// Angular frequency of the target
    Float targetRotation = 0._f;

    /// Number of target particles
    Size targetParticleCnt = 100000;

    /// Minimal number of particles per body
    Size minParticleCnt = 100;

    /// Number of impactor particles.
    ///
    /// If unused (value is NOTHING), the number of particles is automatically computed based on the
    /// number of target particles and the ratio of target radius to projectile radius.
    Optional<Size> impactorParticleCntOverride = NOTHING;

    /// If true, derivatives in impactor will be computed with lower precision.
    ///
    /// This significantly improves the performance of the code. The option is intended mainly for cratering
    /// impacts and should be always false when simulating collision of bodies of comparable sizes.
    bool optimizeImpactor = true;

    /// If true, positions and velocities of particles are modified so that center of mass is at origin and
    /// has zero velocity.
    bool centerOfMassFrame = false;

    /// Path to the output directory.
    ///
    /// If set, parameters of target and impactor are saved there.
    Path outputPath;

    /// Function specifying particle concentration inside the target.
    ///
    /// If not specified, particles are spaced homogeneously.
    Function<Float(const Vector& r)> concentration;
};

/// \brief Object for setting up initial conditions of asteroid impact.
class Collision : public Noncopyable {
private:
    InitialConditions _ic;
    BodySettings _body;
    CollisionParams _params;

public:
    Collision(ISolver& solver,
        const RunSettings& settings,
        const BodySettings& body,
        const CollisionParams& params);

    /// \brief Adds a target (primary body) into the storage.
    ///
    /// Can be called only once for given storage.
    void addTarget(Storage& storage);

    /// \brief Adds an impactor (secondary body) into the storage.
    ///
    /// Can be called only once for given storage. Must be called after \ref addTarget has been called.
    void addImpactor(Storage& storage);

    /// \bref Returns the position of the impact point.
    Vector getImpactPoint() const;
};

} // namespace Presets

NAMESPACE_SPH_END
