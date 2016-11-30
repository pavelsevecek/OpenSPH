#pragma once

/// Standard SPH solver, using density and specific energy as independent variables. Evolves density using
/// continuity equation and energy using energy equation. Works with any artificial viscosity and any equation
/// of state.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"
#include "objects/containers/ArrayUtils.h"
#include "objects/finders/Finder.h"
#include "physics/Eos.h"
#include "solvers/AbstractSolver.h"
#include "sph/av/Standard.h"
#include "sph/boundary/Boundary.h"
#include "sph/distributions/Distribution.h"
#include "sph/kernel/Kernel.h"
#include "storage/Iterate.h"
#include "storage/QuantityKey.h"
#include "system/Factory.h"
#include "system/Profiler.h"
#include "system/Settings.h"


NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Finder;
    class Eos;
}
struct NeighbourRecord;


template <int d, typename TForce>
class ContinuitySolver : public SolverBase<d> {
private:
    TForce force;
    std::unique_ptr<Abstract::Eos> eos;

public:
    ContinuitySolver(const Settings<GlobalSettingsIds>& settings)
        : SolverBase<d>(settings) {

        /// \todo we have to somehow connect EoS with storage. Plus when two storages are merged, we have to
        /// remember EoS for each particles. This should be probably generalized, for example we want to
        /// remember
        /// original body of the particle, possibly original position (right under surface, core of the body,
        /// antipode, ...)

        /// \todo !!!toto je ono, tady nejde globalni nastaveni
        eos = Factory::getEos(BODY_SETTINGS);

        std::unique_ptr<Abstract::Domain> domain = Factory::getDomain(settings);
        this->boundary = Factory::getBoundaryConditions(settings, std::move(domain));
    }

    virtual void compute(Storage& storage) override {

        const int size = storage.getParticleCnt();

        ArrayView<Vector> r, dv;
        ArrayView<Float> rho, m;
        // Check that quantities are valid
        ASSERT(areAllMatching(dv, [](const Vector v) { return v == Vector(0._f); }));
        ASSERT(areAllMatching(rho, [](const Float v) { return v > 0._f; }));

        // build neighbour finding object
        /// \todo only rebuild(), try to limit allocations
        PROFILE_SCOPE("ContinuitySolver::compute (init)")
        this->finder->build(r);

        // we symmetrize kernel by averaging smoothing lenghts
        SymH<d> w(this->kernel);

        PROFILE_NEXT("ContinuitySolver::compute (main cycle)")
        for (int i = 0; i < size; ++i) {
            // Find all neighbours within kernel support. Since we are only searching for particles with
            // smaller h, we know that symmetrized lengths (h_i + h_j)/2 will be ALWAYS smaller or equal to
            // h_i, and we thus never "miss" a particle.
            SCOPE_STOP;
            this->finder->findNeighbours(i, r[i][H] * this->kernel.radius(), this->neighs, FinderFlags::FIND_ONLY_SMALLER_H);
            SCOPE_RESUME;
            // iterate over neighbours
            for (const auto& neigh : this->neighs) {
                const int j = neigh.index;
                // actual smoothing length
                const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
                ASSERT(hbar > EPS && hbar <= r[i][H]);
                if (getSqrLength(r[i] - r[j]) > Math::sqr(this->kernel.radius() * hbar)) {
                    // aren't actual neighbours
                    continue;
                }
                // compute gradient of kernel W_ij
                const Vector grad = w.grad(r[i], r[j]);
                ASSERT(dot(grad, r[i] - r[j]) <= 0._f);

                const Vector f = force(i, j);

                ASSERT(Math::isReal(f));
                dv[i] -= m[j] * f; // opposite sign due to antisymmetry of gradient
                dv[j] += m[i] * f;

                //process(grad); // nothing should depend on Wij
            }
        }
    }
};

NAMESPACE_SPH_END
