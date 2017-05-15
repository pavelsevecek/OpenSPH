#pragma once

/// \file Diagnostics.h
/// \brief Looking for problems in SPH simulation and reporting potential errors
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz) 
/// \date 2016-2017

#include "objects/wrappers/Outcome.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

/// \todo incorporate into EquationTerm / Derivative framework

namespace Abstract {
	/// Error message of diagnostics
	class DiagnosticsReport {
		std::string message;
		Array<Size> offendingParticles;
	};
	
    /// Base class of diagnostics of the run. Compared to Abstract::Integral, the diagnostics shall return
    /// boolean result, indicating whether everything is OK an error occured.    
    class Diagnostics : public Polymorphic {
    public:
        virtual BasicOutcome<DiagnosticsReport> check(const Storage& storage) = 0;
    };
}

class ParticlePairing : public Abstract::Diagnostics {
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
    ParticlePairing(const Float radius = 2._f, const Float limit = 1.e-2_f)
        : radius(radius)
        , limit(limit) {}

    /// Returns the list of particles forming pairs, i.e. particles on top of each other or very close. If
    /// the array is not empty, this is a sign of pairing instability or multi-valued velocity field, both
    /// unwanted artefacts in SPH simulations.
    /// This might occur because of numerical instability, possibly due to time step being too high, or
    /// due to division by very small number in evolution equations. If the pairing instability occurs
    /// regardless, try choosing different parameter SPH_KERNEL_ETA (should be aroung 1.5), or by choosing
    /// different SPH kernel.
    /// \returns Detected pairs of particles given by their indices in the array, in no particular order.
    Array<Pair> getPairs(const Storage& storage) const;

    /// Checks for particle pairs, returns SUCCESS if no pair is found.
    virtual virtual BasicOutcome<DiagnosticsReport> check(const Storage& storage) override;
};

/// Checks for large differences of smoothing length between neighbouring particles
class SmoothingDiscontinuity : public Abstract::Diagnostics {
    Float radius;
    Float limit;

public:
    /// \param limit Limit of relative difference defining the discontinuity. If smoothing lengths h[i] and
    /// h[j] satisfy inequality abs(h[i] - h[j]) > limit * (h[i] + h[j]), an error is reported.
    SmoothingDiscontinuity(const Float radius, const Float limit = 0.5_f)
        : radius(radius)
        , limit(limit) {}

    virtual Outcome check(Storage& storage) override;
};

NAMESPACE_SPH_END
