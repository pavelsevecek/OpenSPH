#pragma once

#include "objects/finders/NeighbourFinder.h"
#include "physics/Eos.h"
#include "quantities/Quantity.h"
#include "sph/Materials.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"
#include "thread/ThreadLocal.h"
#include "timestepping/ISolver.h"

NAMESPACE_SPH_BEGIN

/// \brief Minimalistic SPH solver, mainly used for benchmarking and educational purposes.
class SimpleSolver : public ISolver {
private:
    AutoPtr<ISymmetricFinder> finder;

    /// Scheduler used to parallelize the solver.
    IScheduler& scheduler;

    /// Selected SPH kernel
    LutKernel<DIMENSIONS> kernel;

    struct ThreadData {
        /// Cached array of neighbours, to avoid allocation every step
        Array<NeighbourRecord> neighs;

        /// Indices of real neighbours
        Array<Size> idxs;

        /// Cached array of gradients
        Array<Vector> grads;
    };

    ThreadLocal<ThreadData> threadData;

public:
    SimpleSolver(IScheduler& scheduler, const RunSettings& settings)
        : scheduler(scheduler)
        , threadData(scheduler) {
        finder = Factory::getFinder(settings);
        kernel = Factory::getKernel<DIMENSIONS>(settings);
    }

    virtual void integrate(Storage& storage, Statistics& UNUSED(stats)) override {
        // compute pressure and sound speed using equation of state
        IMaterial& material = storage.getMaterial(0);
        const IEos& eos = dynamic_cast<EosMaterial&>(material).getEos();
        ArrayView<const Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
        ArrayView<const Float> u = storage.getValue<Float>(QuantityId::ENERGY);
        ArrayView<Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
        ArrayView<Float> cs = storage.getValue<Float>(QuantityId::SOUND_SPEED);
        parallelFor(scheduler, 0, p.size(), [&](const Size i) { //
            tie(p[i], cs[i]) = eos.evaluate(rho[i], u[i]);
        });

        // build the structure for finding neighbors
        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        finder->build(scheduler, r);

        // find the largest smoothing length
        const auto maxHIter = std::max_element(
            r.begin(), r.end(), [](const Vector& r1, const Vector& r2) { return r1[H] < r2[H]; });
        const Float searchRadius = (*maxHIter)[H] * kernel.radius();

        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
        ArrayView<Vector> dv = storage.getD2t<Vector>(QuantityId::POSITION);
        ArrayView<Float> drho = storage.getDt<Float>(QuantityId::DENSITY);
        ArrayView<Float> du = storage.getDt<Float>(QuantityId::ENERGY);
        auto functor = [&](Size i, ThreadData& data) {
            finder->findAll(r[i], searchRadius, data.neighs);
            for (const NeighbourRecord& n : data.neighs) {
                const Size j = n.index;
                const Float h_bar = 0.5_f * (r[i][H] + r[j][H]);
                if (sqr(h_bar * kernel.radius()) > n.distanceSqr) {
                    // not actual neighbor
                    continue;
                }

                const Vector grad = kernel.grad(r[j] - r[i], h_bar);

                // equation of motion
                dv[i] -= m[j] * (p[i] / sqr(rho[i]) + p[j] / sqr(rho[j])) * grad;

                // continuity equation
                drho[i] += m[j] * dot(v[j] - v[i], grad);

                // energy equation
                du[i] += m[j] * p[i] / sqr(rho[i]) * dot(v[j] - v[i], grad);
            }
        };
        parallelFor(scheduler, threadData, 0, r.size(), functor);
    }

    virtual void create(Storage& storage, IMaterial& material) const override {
        storage.insert<Float>(
            QuantityId::ENERGY, OrderEnum::FIRST, material.getParam<Float>(BodySettingsId::ENERGY));
        material.setRange(QuantityId::ENERGY, BodySettingsId::ENERGY_RANGE, BodySettingsId::ENERGY_MIN);

        storage.insert<Float>(
            QuantityId::DENSITY, OrderEnum::FIRST, material.getParam<Float>(BodySettingsId::DENSITY));
        material.setRange(QuantityId::DENSITY, BodySettingsId::DENSITY_RANGE, BodySettingsId::DENSITY_MIN);

        storage.insert<Float>(QuantityId::PRESSURE, OrderEnum::ZERO, 0._f);
        storage.insert<Float>(QuantityId::SOUND_SPEED, OrderEnum::ZERO, 0._f);
    }
};

NAMESPACE_SPH_END
