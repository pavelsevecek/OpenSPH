#include "sph/equations/Derivative.h"
#include "objects/containers/FlatMap.h"

NAMESPACE_SPH_BEGIN

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
    idxs = input.getValue<Size>(QuantityId::FLAG);
    tie(m, rho, reduce) =
        input.getValues<Float>(QuantityId::MASS, QuantityId::DENSITY, QuantityId::STRESS_REDUCING);
    C = results.getBuffer<SymmetricTensor>(QuantityId::STRAIN_RATE_CORRECTION_TENSOR, OrderEnum::ZERO);
}

void CorrectionTensor::evalNeighs(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
    ASSERT(neighs.size() == grads.size());
    C[i] = SymmetricTensor::null();
    for (Size k = 0; k < neighs.size(); ++k) {
        const Size j = neighs[k];
        if (idxs[i] != idxs[j] || reduce[i] == 0._f || reduce[j] == 0._f) {
            // condition must match the one in velocity template!
            continue;
        }
        SymmetricTensor t = outer(r[j] - r[i], grads[k]); // symmetric in i,j ?
        C[i] += m[j] / rho[j] * t;
    }
    if (C[i] == SymmetricTensor::null()) {
        C[i] = SymmetricTensor::identity();
    } else {
        // sanity check that we are not getting 'weird' tensors with non-positive values on diagonal
        ASSERT(minElement(C[i].diagonal()) >= 0._f, C[i]);
        if (C[i].determinant() > 0.01_f) {
            C[i] = C[i].inverse();
            ASSERT(C[i].determinant() > 0._f, C[i]);
        } else {
            C[i] = SymmetricTensor::identity(); // C[i].pseudoInverse(1.e-3_f);
        }
    }
    ASSERT(C[i] != SymmetricTensor::null());
}


void DerivativeHolder::require(AutoPtr<IDerivative>&& derivative) {
    for (const AutoPtr<IDerivative>& d : derivatives) {
        // we need to create temporary references, otherwise clang will complain
        const IDerivative& value1 = *d;
        const IDerivative& value2 = *derivative;
        if (typeid(value1) == typeid(value2)) {
            // same type, do nothing
            return;
        }
    }
    derivatives.insert(std::move(derivative));
}

void DerivativeHolder::initialize(const Storage& input) {
    if (needsCreate) {
        // lazy buffer creation
        for (const auto& deriv : derivatives) {
            deriv->create(accumulated);
        }
        needsCreate = false;
    }
    // initialize buffers first, possibly resizing then and invalidating previously stored arrayviews
    accumulated.initialize(input.getParticleCnt());

    for (const auto& deriv : derivatives) {
        // then get the arrayviews for derivatives
        deriv->initialize(input, accumulated);
    }
}

NAMESPACE_SPH_END
