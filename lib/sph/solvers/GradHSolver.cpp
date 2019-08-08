#include "sph/solvers/GradHSolver.h"
#include "objects/finders/NeighbourFinder.h"
#include "quantities/Quantity.h"

NAMESPACE_SPH_BEGIN

class AsymmetricPressureGradient : public IAsymmetricDerivative {
    ArrayView<const Float> p, m, rho;
    ArrayView<Vector> dv;

public:
    virtual void create(Accumulated& results) override {
        results.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, BufferSource::SHARED);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        tie(p, m, rho) = input.getValues<Float>(QuantityId::PRESSURE, QuantityId::MASS, QuantityId::DENSITY);
        dv = results.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
    }

    virtual void evalAsymmetric(const Size i,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> gradi,
        ArrayView<const Vector> gradj) override {
        ASSERT(neighs.size() == gradi.size() && neighs.size() == gradj.size());
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];
            const Vector f = p[i] / sqr(rho[i]) * gradi[k] + p[j] / sqr(rho[j]) * gradj[k];
            ASSERT(isReal(f));
            dv[i] -= m[j] * f;
        }
    }
};

/// \brief Object evaluating grad-h terms.
///
/// Does not derive from \ref IDerivative interface as there is currently no need for it; it is evaluated
/// alone in the pre-pass.
class GradH {
private:
    ArrayView<const Vector> r;
    ArrayView<const Float> rho;
    ArrayView<Float> omega;

public:
    explicit GradH(Storage& storage) {
        omega = storage.getValue<Float>(QuantityId::GRAD_H);
        rho = storage.getValue<Float>(QuantityId::DENSITY);
        r = storage.getValue<Vector>(QuantityId::POSITION);
    }

    void eval(const LutKernel<DIMENSIONS>& kernel, const Size i, ArrayView<const NeighbourRecord> neighs) {
        Float sum = 0._f;
        for (const NeighbourRecord& n : neighs) {
            const Size j = n.index;
            const Vector r_ji = r[j] - r[i];
            const Float h_j = r[j][H];
            const Float dWij_dh =
                -dot(r_ji, kernel.grad(r_ji, h_j)) - DIMENSIONS / h_j * kernel.value(r_ji, h_j);
            sum += dWij_dh;
        }
        // add term for i=j (right?)
        sum += -DIMENSIONS / r[i][H] * kernel.value(Vector(0._f), r[i][H]);

        omega[i] = 1._f + r[i][H] / (3._f * rho[i]) * sum;
        // For const smoothing lengths, omega should be 1. Possibly relax this assert if the real values are
        // outside the expected range.
        ASSERT(isReal(omega[i]) && omega[i] > 0.5_f && omega[i] < 2._f);
    }
};

GradHSolver::GradHSolver(IScheduler& scheduler,
    const RunSettings& settings,
    const EquationHolder& basicTerms,
    Array<AutoPtr<IAsymmetricTerm>>&& asymmetricTerms)
    : AsymmetricSolver(scheduler, settings, basicTerms)
    , secondData(scheduler) {
    for (auto& term : asymmetricTerms) {
        term->setAsymmetricDerivatives(asymmetricDerivatives);
    }
}

void GradHSolver::create(Storage& storage, IMaterial& material) const {
    AsymmetricSolver::create(storage, material);

    storage.insert<Float>(QuantityId::GRAD_H, OrderEnum::ZERO, 1._f);
}

void GradHSolver::loop(Storage& storage, Statistics& UNUSED(stats)) {
    // initialize all asymmetric derivatives
    for (auto& deriv : asymmetricDerivatives) {
        deriv->initialize(storage, derivatives.getAccumulated());
    }

    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    const IBasicFinder& finder = getFinder(r);

    // find the maximum search radius
    Float maxH = 0._f;
    for (Size i = 0; i < r.size(); ++i) {
        maxH = max(maxH, r[i][H]);
    }
    const Float radius = maxH * kernel.radius();

    // compute the grad-h terms
    GradH gradH(storage);
    auto preFunctor = [this, radius, &finder, &gradH](const Size i, ThreadData& data) {
        finder.findAll(i, radius, data.neighs);
        gradH.eval(kernel, i, data.neighs);
    };
    parallelFor(scheduler, threadData, 0, r.size(), preFunctor);

    ArrayView<Size> neighs = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);
    ArrayView<Float> omega = storage.getValue<Float>(QuantityId::GRAD_H);

    auto functor = [this, r, &finder, &neighs, omega, radius](const Size i, ThreadData& data) {
        finder.findAll(i, radius, data.neighs);
        data.idxs.clear();
        data.grads.clear();

        Array<Vector>& secondGrads = secondData.local().grads;
        secondGrads.clear();

        for (auto& n : data.neighs) {
            const Size j = n.index;
            const Vector gradi = 1._f / omega[i] * kernel.grad(r[i] - r[j], r[i][H]);
            ASSERT(isReal(gradi) && dot(gradi, r[i] - r[j]) <= 0._f, gradi, r[i] - r[j]);
            const Vector gradj = 1._f / omega[j] * kernel.grad(r[j] - r[i], r[j][H]);
            ASSERT(isReal(gradj) && dot(gradj, r[j] - r[i]) <= 0._f, gradj, r[j] - r[i]);
            data.idxs.emplaceBack(j);
            data.grads.emplaceBack(gradi);
            secondGrads.emplaceBack(gradj);
        }

        // evaluate the 'normal' derivatives using the gradient for i-th particle
        derivatives.eval(i, data.idxs, data.grads);

        // evaluate the 'extra' derivative using both lists of gradients
        for (auto& deriv : asymmetricDerivatives) {
            deriv->evalAsymmetric(i, data.idxs, data.grads, secondGrads);
        }

        neighs[i] = data.idxs.size();
    };
    parallelFor(scheduler, threadData, 0, r.size(), functor);
}

NAMESPACE_SPH_END
