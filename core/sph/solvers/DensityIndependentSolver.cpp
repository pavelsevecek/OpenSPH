#include "sph/solvers/DensityIndependentSolver.h"
#include "objects/Exceptions.h"
#include "objects/finders/NeighborFinder.h"
#include "sph/Materials.h"
#include "sph/equations/av/Standard.h"
#include "system/Factory.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

/// \todo change to simpe IDerivative ? (no need for symmetrization)
class DensityIndependentPressureGradient : public DerivativeTemplate<DensityIndependentPressureGradient> {
private:
    ArrayView<const Vector> v;
    ArrayView<const Float> m;
    ArrayView<const Float> y;
    ArrayView<const Float> Y;

    ArrayView<Vector> dv;
    ArrayView<Float> du;

public:
    DensityIndependentPressureGradient(const RunSettings& settings)
        : DerivativeTemplate<DensityIndependentPressureGradient>(settings) {}

    INLINE void additionalCreate(Accumulated& results) {
        results.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, BufferSource::SHARED);
        results.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, BufferSource::SHARED);
    }

    INLINE void additionalInitialize(const Storage& input, Accumulated& results) {
        tie(m, y, Y) = input.getValues<Float>(
            QuantityId::MASS, QuantityId::GENERALIZED_PRESSURE, QuantityId::GENERALIZED_ENERGY);
        v = input.getDt<Vector>(QuantityId::POSITION);

        dv = results.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
        du = results.getBuffer<Float>(QuantityId::ENERGY, OrderEnum::FIRST);
    }

    INLINE bool additionalEquals(const DensityIndependentPressureGradient& UNUSED(other)) const {
        return true;
    }

    template <bool Symmetrize>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        SPH_ASSERT(!Symmetrize);

        dv[i] += Y[i] * Y[j] / m[i] * (1._f / y[i] + 1._f / y[j]) * grad;
        SPH_ASSERT(getSqrLength(dv[i]) < LARGE, dv[i]);

        du[i] += Y[i] * Y[j] / m[i] / y[i] * dot(v[i] - v[j], grad);
        SPH_ASSERT(abs(du[i]) < LARGE, du[i]);
    }
};

class DensityIndependentPressureForce : public IEquationTerm {
public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require(makeAuto<DensityIndependentPressureGradient>(settings));
    }

    virtual void initialize(IScheduler& UNUSED(scheduler), Storage& storage, const Float UNUSED(t)) override {
        // EoS can return negative pressure, which is not allowed in DISPH, so we have to clamp it
        ArrayView<Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
        for (Size i = 0; i < p.size(); ++i) {
            SPH_ASSERT(p[i] >= 100._f);
            p[i] = max(p[i], 100._f); /// \todo generalize the min value
        }
    }

    virtual void finalize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

    virtual void create(Storage& storage, IMaterial& material) const override {
        if (!dynamic_cast<EosMaterial*>(&material)) {
            throw InvalidSetup("DISPH requires EosMaterial or derived");
        }
        const Float u0 = material.getParam<Float>(BodySettingsId::ENERGY);
        storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, u0);
        material.setRange(QuantityId::ENERGY, BodySettingsId::ENERGY_RANGE, BodySettingsId::ENERGY_MIN);
    }
};

DensityIndependentSolver::DensityIndependentSolver(IScheduler& scheduler, const RunSettings& settings)
    : scheduler(scheduler)
    , threadData(scheduler) {
    finder = Factory::getFinder(settings);
    kernel = Factory::getKernel<3>(settings);

    //    config.alpha = settings.get<Float>(RunSettingsId::SPH_DI_ALPHA);

    equations += makeTerm<DensityIndependentPressureForce>();
    equations += makeTerm<SolidStressForce>(settings);
    equations += makeTerm<StandardAV>();

    equations.setDerivatives(derivatives, settings);
}

DensityIndependentSolver::~DensityIndependentSolver() = default;

void DensityIndependentSolver::integrate(Storage& storage, Statistics& stats) {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    finder->build(scheduler, r);

    // step 1: compute density from the current y and Y
    ArrayView<Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
    ArrayView<const Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<Float> Y = storage.getValue<Float>(QuantityId::GENERALIZED_ENERGY);
    ArrayView<Float> y = storage.getValue<Float>(QuantityId::GENERALIZED_PRESSURE);
    for (Size i = 0; i < r.size(); ++i) {
        rho[i] = m[i] * y[i] / Y[i];
        SPH_ASSERT(rho[i] > 1._f && rho[i] < 1.e4_f, rho[i]);
    }

    // step 2: using computed density, get the non-smoothed pressure from equation of state
    for (Size matId = 0; matId < storage.getMaterialCnt(); ++matId) {
        MaterialView material = storage.getMaterial(matId);
        material->initialize(scheduler, storage, material.sequence());
    }

    const Float t = stats.get<Float>(StatisticsId::RUN_TIME);
    equations.initialize(scheduler, storage, t);
    derivatives.initialize(scheduler, storage);

    // step 3: update Y from the pressure

    for (Size i = 0; i < r.size(); ++i) {
        Y[i] = m[i] * /*pow(p[i], config.alpha) */ p[i] / rho[i];
        SPH_ASSERT(Y[i] > EPS && Y[i] < LARGE, Y[i]);
    }

    const Float radius = kernel.radius() * r[0][H]; /// \todo do correctly

    // step 4: compute y by summing up neighbors
    auto pressureFunc = [this, Y, r, p, &y, radius](const Size i, ThreadData& data) {
        // find neighbors
        finder->findAll(i, radius, data.neighs);

        y[i] = 0._f;
        for (NeighborRecord& n : data.neighs) {
            const Size j = n.index;
            y[i] += Y[j] * kernel.value(r[i], r[j]);
        }
        SPH_ASSERT(y[i] > 0._f);

        //        y[i] = pow(y[i], 1._f / config.alpha - 2._f);
        SPH_ASSERT(y[i] > EPS && y[i] < LARGE, y[i]);
    };
    parallelFor(scheduler, threadData, 0, r.size(), pressureFunc);

    // step 5: using computed y, evaluate equation of motion and energy equation
    auto equationFunc = [this, Y, r, radius](const Size i, ThreadData& data) {
        // find neighbors
        finder->findAll(i, radius, data.neighs);

        // compute kernels and value p^alpha using direct summation
        data.idxs.clear();
        data.grads.clear();
        for (NeighborRecord& n : data.neighs) {
            const Size j = n.index;
            const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
            SPH_ASSERT(hbar > EPS, hbar);
            if (i == j || n.distanceSqr >= sqr(kernel.radius() * hbar)) {
                // aren't actual neighbors
                continue;
            }

            const Vector gr = kernel.grad(r[i], r[j]);
            SPH_ASSERT(isReal(gr) && dot(gr, r[i] - r[j]) <= 0._f, gr, r[i] - r[j]);
            data.grads.emplaceBack(gr);
            data.idxs.emplaceBack(j);
        }

        derivatives.eval(i, data.idxs, data.grads);
    };
    parallelFor(scheduler, threadData, 0, r.size(), equationFunc);

    derivatives.getAccumulated().store(scheduler, storage);
    equations.finalize(scheduler, storage, t);

    // step 6: get an estimate of Y for next time step by computing its derivative
    // dY/dt must be computed after all equations are finalized, as it depends on du/dt
    ArrayView<Float> dY = storage.getDt<Float>(QuantityId::GENERALIZED_ENERGY);
    ArrayView<const Float> cs = storage.getValue<Float>(QuantityId::SOUND_SPEED);
    ArrayView<const Float> du = storage.getDt<Float>(QuantityId::ENERGY);
    for (Size i = 0; i < r.size(); ++i) {
        const Float gamma = rho[i] / p[i] * sqr(cs[i]);
        SPH_ASSERT(gamma > 0._f);
        // y[i] = pow(y[i], 1._f / (1._f / config.alpha - 2._f)); /// \todo
        // const Float y1 = pow(y[i], 1._f - 1._f / config.alpha);
        dY[i] = 0._f * (/*config.alpha * */ gamma - 1._f) * m[i] * du[i];
        SPH_ASSERT(isReal(dY[i]) && abs(dY[i]) < LARGE);
    }

    for (Size matId = 0; matId < storage.getMaterialCnt(); ++matId) {
        MaterialView material = storage.getMaterial(matId);
        material->finalize(scheduler, storage, material.sequence());
    }
}

void DensityIndependentSolver::create(Storage& storage, IMaterial& material) const {
    const Float rho0 = material.getParam<Float>(BodySettingsId::DENSITY);
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, rho0);

    // set up to something which equals y/Y = m/rho
    const Float m0 = storage.getValue<Float>(QuantityId::MASS)[0];
    storage.insert<Float>(QuantityId::GENERALIZED_PRESSURE, OrderEnum::ZERO, rho0);
    storage.insert<Float>(QuantityId::GENERALIZED_ENERGY, OrderEnum::FIRST, m0);
    material.setRange(QuantityId::GENERALIZED_ENERGY, Interval(EPS, INFTY), LARGE);
    equations.create(storage, material);
}

NAMESPACE_SPH_END
