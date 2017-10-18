#pragma once

/// \file NBodySolver.h
/// \brief Solver performing N-body simulation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gravity/IGravity.h"
#include "system/Settings.h"
#include "timestepping/ISolver.h"

NAMESPACE_SPH_BEGIN

/// \brief Solver computing gravitational interaction of particles.
///
/// The particles are assumed to be point masses. No hydrodynamics or collisions are considered.
class NBodySolver : public ISolver {
private:
    /// Gravity used by the solver
    AutoPtr<IGravity> gravity;

    /// Thread pool for parallelization
    ThreadPool pool;

public:
    explicit NBodySolver(const RunSettings& settings, AutoPtr<IGravity>&& gravity)
        : gravity(std::move(gravity))
        , pool(settings.get<int>(RunSettingsId::RUN_THREAD_CNT)) {}

    virtual void integrate(Storage& storage, Statistics& stats) override {
        gravity->build(storage);

        ArrayView<Vector> dv = storage.getD2t<Vector>(QuantityId::POSITIONS);
        gravity->evalAll(pool, dv, stats);
    }

    virtual void create(Storage& storage, IMaterial& material) const override {
        // no quantities need to be created, positions and masses are already in the storage
    }
};

NAMESPACE_SPH_END
