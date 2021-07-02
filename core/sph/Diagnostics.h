#pragma once

/// \file Diagnostics.h
/// \brief Looking for problems in SPH simulation and reporting potential errors
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/wrappers/Outcome.h"
#include "quantities/Storage.h"
#include <map>

NAMESPACE_SPH_BEGIN

/// \brief Object containing a reported error message.
struct DiagnosticsError {
    /// \brief Description of the encountered problem
    std::string description;

    /// \brief Problematic particles and optional error message for each of them.
    ///
    /// The per-particle message can be empty.
    std::map<Size, std::string> offendingParticles;
};

using DiagnosticsReport = BasicOutcome<DiagnosticsError>;

/// \brief Base class of diagnostics of the run.
///
/// Compared to \brief IIntegral, the diagnostics returns a boolean result, indicating whether everything is
/// OK or an error occured.
class IDiagnostic : public Polymorphic {
public:
    virtual DiagnosticsReport check(const Storage& storage, const Statistics& stats) const = 0;
};

/// \brief Checks for particle pairs, indicating a pairing instability.
class ParticlePairingDiagnostic : public IDiagnostic {
private:
    Float radius;
    Float limit;

public:
    struct Pair {
        Size i1, i2;
    };

    /// \param radius Search radius for pairs in units of smoothing length. This should correspond to radius
    ///               of selected smoothing kernel.
    /// \param limit Maximal distance of two particles forming a pair in units of smoothing length.
    explicit ParticlePairingDiagnostic(const Float radius = 2._f, const Float limit = 1.e-2_f)
        : radius(radius)
        , limit(limit) {}

    /// \brief Returns the list of particles forming pairs, i.e. particles on top of each other or very close.
    ///
    /// If the array is not empty, this is a sign of pairing instability or multi-valued velocity field, both
    /// unwanted artefacts in SPH simulations. This might occur because of numerical instability, possibly due
    /// to time step being too high, or due to division by very small number in evolution equations. If the
    /// pairing instability occurs regardless, try choosing different parameter SPH_KERNEL_ETA (should be
    /// aroung 1.5), or by choosing different SPH kernel.
    ///
    /// \returns Detected pairs of particles given by their indices in the array, in no particular order.
    Array<Pair> getPairs(const Storage& storage) const;

    /// \brief Checks for particle pairs, returns SUCCESS if no pair is found.
    virtual DiagnosticsReport check(const Storage& storage, const Statistics& stats) const override;
};

/// \brief Checks for large differences of smoothing length between neighbouring particles.
class SmoothingDiscontinuityDiagnostic : public IDiagnostic {
    Float radius;
    Float limit;

public:
    /// \param limit Limit of relative difference defining the discontinuity. If smoothing lengths h[i] and
    /// h[j] satisfy inequality abs(h[i] - h[j]) > limit * (h[i] + h[j]), an error is reported.
    SmoothingDiscontinuityDiagnostic(const Float radius, const Float limit = 0.5_f)
        : radius(radius)
        , limit(limit) {}

    virtual DiagnosticsReport check(const Storage& storage, const Statistics& stats) const override;
};

/// \brief Checks for excessively large magnitudes of acceleration, indicating a numerical instability.
///
/// This is usually caused by violating the CFL criterion. To resolve the problem, try decreasing the Courant
/// number of the simulation.
class CourantInstabilityDiagnostic : public IDiagnostic {
private:
    Float factor;

public:
    /// \param factor Limit of the acceleration (in seconds).
    explicit CourantInstabilityDiagnostic(const Float timescaleFactor);

    virtual DiagnosticsReport check(const Storage& storage, const Statistics& stats) const override;
};

/// \brief Checks for clamping of excesivelly low values of internal energy.
///
/// This breaks the conservation of total energy and suggests a problem in the simulation setup.
class OvercoolingDiagnostic : public IDiagnostic {
public:
    virtual DiagnosticsReport check(const Storage& storage, const Statistics& stats) const override;
};

NAMESPACE_SPH_END
