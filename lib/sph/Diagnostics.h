#pragma once

#include "objects/wrappers/Outcome.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN


namespace Abstract {
    /// Base class of diagnostics of the run. Compared to Abstract::Integral, the diagnostics shall return
    /// boolean result, indicating whether everything is OK an error occured.
    /// \todo add offending particles directly into interface rather than output message?
    class Diagnostics : public Polymorphic {
    public:
        virtual Outcome check(Storage& storage) = 0;
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
    Array<Pair> getPairs(Storage& storage) const;

    /// Checks for particle pairs, returns SUCCESS if no pair is found.
    virtual Outcome check(Storage& storage) override {
        Array<Pair> pairs = getPairs(storage);
        if (!pairs.empty()) {
            std::string message = "Particle pairs found: \n";
            for (Pair& p : pairs) {
                message += " - particles " + std::to_string(p.i1) + " and " + std::to_string(p.i2);
            }
            return message;
        }
        return SUCCESS;
    }
};

/// Checks for large differences of smoothing length between neighbouring particles
class SmoothingDiscontinuity : public Abstract::Diagnostics {
    Float limit;

public:
    /// Limit relative difference defining the discontinuity. If for smoothing lengths h[i] and h[j]:
    /// abs(h[i] - h[j]) > limit * (h[i] + h[j]), an error is reported.
    SmoothingDiscontinuity(const Float limit = 0.5_f)
        : limit(limit) {}

    virtual Outcome check(Storage& storage) override;
};

NAMESPACE_SPH_END
