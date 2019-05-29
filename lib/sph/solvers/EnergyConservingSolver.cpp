#include "sph/solvers/EnergyConservingSolver.h"
#include "io/Logger.h"

NAMESPACE_SPH_BEGIN

// ----------------------------------------------------------------------------------------------------------
// Energy partitioners
// ----------------------------------------------------------------------------------------------------------

class Equipartitioner : public IEnergyPartitioner {
public:
    virtual void initialize(const Storage& UNUSED(storage)) override {}

    virtual void compute(const Size UNUSED(i),
        ArrayView<const Size> UNUSED(neighs),
        ArrayView<const Float> UNUSED(e),
        ArrayView<Float> f) const override {
        for (Size k = 0; k < f.size(); ++k) {
            f[k] = 0.5_f;
        }
    }
};

class SmoothlyDiminishingPartitioner : public IEnergyPartitioner {
private:
    ArrayView<const Float> m;
    ArrayView<const Float> u;


public:
    virtual void initialize(const Storage& storage) override {
        m = storage.getValue<Float>(QuantityId::MASS);
        u = storage.getValue<Float>(QuantityId::ENERGY);
    }

    virtual void compute(const Size i,
        ArrayView<const Size> neighs,
        ArrayView<const Float> e,
        ArrayView<Float> f) const override {
        for (Size k = 0; k < f.size(); ++k) {
            const Size j = neighs[k];
            const Float u_ji = u[j] - u[i];
            f[k] = 0.5_f * (1._f + u_ji * sgn(e[k]) / (abs(u_ji) + 1._f / (1._f + abs(u_ji))));
        }
    }
};

class MonotonicDiminishingPartitioner : public IEnergyPartitioner {
private:
    ArrayView<const Float> m;
    ArrayView<const Float> u;

public:
    virtual void initialize(const Storage& storage) override {
        m = storage.getValue<Float>(QuantityId::MASS);
        u = storage.getValue<Float>(QuantityId::ENERGY);
    }

    virtual void compute(const Size i,
        ArrayView<const Size> neighs,
        ArrayView<const Float> e,
        ArrayView<Float> f) const override {
        for (Size k = 0; k < f.size(); ++k) {
            const Size j = neighs[k];
            const Float u_ji = u[j] - u[i];
            if (u_ji != 0._f) {
                const Float A = e[k] / u_ji;
                const Float B = (A >= 0._f) ? A / m[i] : A / m[j];
                ASSERT(isReal(A) && isReal(B));
                if (abs(B) <= 1._f) {
                    f[k] = max(0, sgn(B));
                    continue;
                }
            } else if (e[k] == 0._f) {
                // what to do?
                f[k] = 0.5_f;
                continue;
            }
            // either abs(B) > 1 or u_ji == 0
            f[k] = m[i] / e[k] * ((e[k] + m[i] * u[i] + m[j] * u[j]) / (m[i] + m[j]) - u[i]);
            ASSERT(isReal(f[k]), e[k], u[i], u[j]);
        }
    }
};

template <typename TPrimary, typename TSecondary>
class BlendingPartitioner : public IEnergyPartitioner {
private:
    TPrimary primary;
    TSecondary secondary;

    ArrayView<const Float> u;

public:
    virtual void initialize(const Storage& storage) override {
        primary.initialize(storage);
        secondary.initialize(storage);

        u = storage.getValue<Float>(QuantityId::ENERGY);
    }

    virtual void compute(const Size i,
        ArrayView<const Size> neighs,
        ArrayView<const Float> e,
        ArrayView<Float> f) const override {
        for (Size k = 0; k < f.size(); ++k) {
            const Size j = neighs[k];
            const Float chi = abs(u[j] - u[i]) / (abs(u[i]) + abs(u[j]) + EPS);

            Float f1;
            primary.compute(i, getSingleValueView(j), getSingleValueView(e[k]), getSingleValueView(f1));
            Float f2;
            secondary.compute(i, getSingleValueView(j), getSingleValueView(e[k]), getSingleValueView(f2));
            f[k] = lerp(f1, f2, chi);
            ASSERT(f[k] >= 0._f && f[k] <= 1._f, f[k]);
        }
    }
};

// ----------------------------------------------------------------------------------------------------------
// EnergyConservingSolver implementation
// ----------------------------------------------------------------------------------------------------------

EnergyConservingSolver::EnergyConservingSolver(IScheduler& scheduler,
    const RunSettings& settings,
    const EquationHolder& eqs)
    : IAsymmetricSolver(scheduler, settings, eqs)
    , threadData(scheduler) {
    initialDt = settings.get<Float>(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP);

    partitioner =
        makeAuto<BlendingPartitioner<SmoothlyDiminishingPartitioner, MonotonicDiminishingPartitioner>>();

    eqs.setDerivatives(derivatives, settings);
}

EnergyConservingSolver::EnergyConservingSolver(IScheduler& scheduler,
    const RunSettings& settings,
    const EquationHolder& eqs,
    AutoPtr<IBoundaryCondition>&& bc)
    : EnergyConservingSolver(scheduler, settings, eqs) {
    if (bc != nullptr) {
        throw InvalidSetup("ECS does not support boundary conditions yet");
    }
}

void EnergyConservingSolver::loop(Storage& storage, Statistics& UNUSED(stats)) {
    MEASURE_SCOPE("EnergyConservingSolver::loop");

    ArrayView<const Vector> r, v, dummy;
    tie(r, v, dummy) = storage.getAll<Vector>(QuantityId::POSITION);

    // we need to symmetrize kernel in smoothing lenghts to conserve momentum
    SymmetrizeSmoothingLengths<LutKernel<DIMENSIONS>&> symmetrizedKernel(kernel);

    const Float radius = this->getSearchRadius(storage);

    const IBasicFinder& finder = this->getFinder(r);
    // partitioner->initialize(storage);

    neighList.resize(r.size());
    gradList.resize(r.size());

    auto evalDerivatives = [&](const Size i, ThreadData& data) {
        finder.findAll(i, radius, data.neighs);

        neighList[i].clear();
        gradList[i].clear();

        for (auto& n : data.neighs) {
            const Size j = n.index;
            const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
            ASSERT(hbar > EPS, hbar);
            if (i == j || getSqrLength(r[i] - r[j]) >= sqr(kernel.radius() * hbar)) {
                // aren't actual neighbours
                continue;
            }
            const Vector gr = symmetrizedKernel.grad(r[i], r[j]);
            ASSERT(isReal(gr) && dot(gr, r[i] - r[j]) < 0._f, gr, r[i] - r[j]);

            neighList[i].push(j);
            gradList[i].push(gr);
        }

        derivatives.eval(i, neighList[i], gradList[i]);
    };
    parallelFor(scheduler, threadData, 0, r.size(), evalDerivatives);
}

void EnergyConservingSolver::beforeLoop(Storage& storage, Statistics& UNUSED(stats)) {
    equations.initialize(scheduler, storage);
    derivatives.initialize(storage);
    const Size particleCnt = storage.getParticleCnt();
    for (ThreadData& data : threadData) {
        data.energyChange.resize(particleCnt);
        data.energyChange.fill(0._f);
    }
}

void EnergyConservingSolver::afterLoop(Storage& storage, Statistics& stats) {
    MEASURE_SCOPE("EnergyConservingSolver::afterLoop");

    Accumulated& accumulated = derivatives.getAccumulated();
    accumulated.store(storage);
    equations.finalize(scheduler, storage);

    // now, we have computed everything that modifies the energy derivatives, so we can override it with stuff
    // below
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);

    // we need to symmetrize kernel in smoothing lenghts to conserve momentum
    SymmetrizeSmoothingLengths<LutKernel<DIMENSIONS>&> symmetrizedKernel(kernel);

    partitioner->initialize(storage);

    /// \todo maybe simply pass the timestep into the function?
    Float dt = stats.getOr<Float>(StatisticsId::TIMESTEP_VALUE, initialDt);
    ArrayView<Float> du = storage.getDt<Float>(QuantityId::ENERGY);

    auto evalAccelerations = [&](const Size i, ThreadData& data) {
        data.accelerations.resize(neighList[i].size());
        derivatives.evalAccelerations(i, neighList[i], gradList[i], data.accelerations);

        data.energyChange.resize(neighList[i].size());
        for (Size k = 0; k < neighList[i].size(); ++k) {
            const Size j = neighList[i][k];
            const Vector vi12 = v[i] + 0.5_f * dv[i] * dt;
            const Vector vj12 = v[j] + 0.5_f * dv[j] * dt;

            data.energyChange[k] = m[i] * dot(vj12 - vi12, data.accelerations[k]);
        }

        data.partitions.resize(neighList[i].size());
        partitioner->compute(i, neighList[i], data.energyChange, data.partitions);

        du[i] = 0._f;
        for (Size k = 0; k < neighList[i].size(); ++k) {
            du[i] += data.partitions[k] * data.energyChange[k] / m[i];
        }
        ASSERT(isReal(du[i]));
    };
    parallelFor(scheduler, threadData, 0, r.size(), evalAccelerations);
}

void EnergyConservingSolver::sanityCheck(const Storage& UNUSED(storage)) const {}

NAMESPACE_SPH_END
