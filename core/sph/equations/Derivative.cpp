#include "sph/equations/Derivative.h"
#include "objects/Exceptions.h"
#include "objects/containers/FlatMap.h"
#include "quantities/Quantity.h"

NAMESPACE_SPH_BEGIN

CorrectionTensor::CorrectionTensor(const RunSettings& settings) {
    sumOnlyUndamaged = settings.get<bool>(RunSettingsId::SPH_SUM_ONLY_UNDAMAGED);
}

DerivativePhase CorrectionTensor::phase() const {
    // needs to be computed first, so that other derivatives can use the result
    return DerivativePhase::PRECOMPUTE;
}

void CorrectionTensor::create(Accumulated& results) {
    results.insert<SymmetricTensor>(
        QuantityId::STRAIN_RATE_CORRECTION_TENSOR, OrderEnum::ZERO, BufferSource::UNIQUE);
}

void CorrectionTensor::initialize(const Storage& input, Accumulated& results) {
    r = input.getValue<Vector>(QuantityId::POSITION);
    tie(m, rho) = input.getValues<Float>(QuantityId::MASS, QuantityId::DENSITY);

    if (sumOnlyUndamaged && input.has(QuantityId::STRESS_REDUCING)) {
        idxs = input.getValue<Size>(QuantityId::FLAG);
        reduce = input.getValue<Float>(QuantityId::STRESS_REDUCING);
    } else {
        reduce = nullptr;
    }

    C = results.getBuffer<SymmetricTensor>(QuantityId::STRAIN_RATE_CORRECTION_TENSOR, OrderEnum::ZERO);
}

void CorrectionTensor::evalNeighs(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
    SPH_ASSERT(neighs.size() == grads.size());
    C[i] = SymmetricTensor::null();
    if (reduce) {
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];
            if (idxs[i] != idxs[j] || reduce[i] == 0._f || reduce[j] == 0._f) {
                // condition must match the one in velocity template!
                continue;
            }
            SymmetricTensor t = symmetricOuter(r[j] - r[i], grads[k]); // symmetric in i,j ?
            C[i] += m[j] / rho[j] * t;
        }
    } else {
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];
            SymmetricTensor t = symmetricOuter(r[j] - r[i], grads[k]);
            C[i] += m[j] / rho[j] * t;
        }
    }

    if (C[i] == SymmetricTensor::null()) {
        C[i] = SymmetricTensor::identity();
    } else {
        // sanity check that we are not getting 'weird' tensors with non-positive values on diagonal
        SPH_ASSERT(minElement(C[i].diagonal()) >= 0._f, C[i]);
        if (C[i].determinant() > 0.01_f) {
            C[i] = C[i].inverse();
            SPH_ASSERT(C[i].determinant() > 0._f, C[i]);
        } else {
            /*C[i] = C[i].pseudoInverse(1.e-3_f);
            if (C[i] == SymmetricTensor::null()) {
                C[i] = SymmetricTensor::identity();
            }*/
            C[i] = SymmetricTensor::identity();
        }
    }
    SPH_ASSERT(C[i] != SymmetricTensor::null());
}


void DerivativeHolder::require(AutoPtr<IDerivative>&& derivative) {
    for (const AutoPtr<IDerivative>& d : derivatives) {
        // we need to create temporary references, otherwise clang will complain
        const IDerivative& value1 = *d;
        const IDerivative& value2 = *derivative;
        if (typeid(value1) == typeid(value2)) {
            // same type, check for equality of derivatives and return
            if (!value1.equals(value2)) {
                throw InvalidSetup(
                    "Using two derivatives with the same type, but with different internal state. This is "
                    "currently unsupported; while it is allowed to require the same derivative more than "
                    "once, it MUST have the same state.");
            }
            return;
        }
    }
    derivatives.insert(std::move(derivative));
}

void DerivativeHolder::initialize(IScheduler& scheduler, const Storage& input) {
    if (needsCreate) {
        // lazy buffer creation
        for (const auto& deriv : derivatives) {
            deriv->create(accumulated);
        }
        needsCreate = false;
    }
    // initialize buffers first, possibly resizing then and invalidating previously stored arrayviews
    accumulated.initialize(scheduler, input.getParticleCnt());

    for (const auto& deriv : derivatives) {
        // then get the arrayviews for derivatives
        deriv->initialize(input, accumulated);
    }
}

void DerivativeHolder::eval(const Size idx, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
    SPH_ASSERT(neighs.size() == grads.size());
    for (const auto& deriv : derivatives) {
        deriv->evalNeighs(idx, neighs, grads);
    }
}

void DerivativeHolder::evalSymmetric(const Size idx,
    ArrayView<const Size> neighs,
    ArrayView<const Vector> grads) {
    SPH_ASSERT(neighs.size() == grads.size());
    SPH_ASSERT(isSymmetric());
    for (const auto& deriv : derivatives) {
        ISymmetricDerivative* symmetric = assertCast<ISymmetricDerivative>(&*deriv);
        symmetric->evalSymmetric(idx, neighs, grads);
    }
}

bool DerivativeHolder::isSymmetric() const {
    for (const auto& deriv : derivatives) {
        if (!dynamic_cast<ISymmetricDerivative*>(&*deriv)) {
            return false;
        }
    }
    return true;
}

/*DerivativeHolder DerivativeHolder::getSubset(const QuantityId id, const OrderEnum order) {
    DerivativeHolder subset;
    for (const auto& deriv : derivatives) {
        Accumulated a;
        deriv->create(a);
        if (a.hasBuffer(id, order)) {
            // required since we don't sort the returned values by phase
            SPH_ASSERT(deriv->phase() == DerivativePhase::EVALUATION);
            subset.derivatives.insert(deriv);
        }
    }
    return subset;
}*/

NAMESPACE_SPH_END
