#include "physics/Integrals.h"
#include "post/Analysis.h"
#include "quantities/Storage.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN


Float TotalMass::evaluate(const Storage& storage) const {
    Float total(0._f);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    SPH_ASSERT(!m.empty());
    for (Size i = 0; i < m.size(); ++i) {
        total += m[i];
    }
    SPH_ASSERT(isReal(total));
    return total;
}

TotalMomentum::TotalMomentum(const Float omega)
    : omega(0._f, 0._f, omega) {}


Vector TotalMomentum::evaluate(const Storage& storage) const {
    BasicVector<double> total(0.); // compute in double precision to avoid round-off error during accumulation
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    SPH_ASSERT(!v.empty());
    for (Size i = 0; i < v.size(); ++i) {
        total += vectorCast<double>(m[i] * (v[i] + cross(omega, r[i])));
    }
    SPH_ASSERT(isReal(total));
    return vectorCast<Float>(total);
}

TotalAngularMomentum::TotalAngularMomentum(const Float omega)
    : frameFrequency(0._f, 0._f, omega) {}

Vector TotalAngularMomentum::evaluate(const Storage& storage) const {
    BasicVector<double> total(0.);
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);

    // angular momentum with respect to origin
    SPH_ASSERT(!v.empty());
    for (Size i = 0; i < v.size(); ++i) {
        total += vectorCast<double>(m[i] * cross(r[i], (v[i] + cross(frameFrequency, r[i]))));
    }

    // local angular momentum
    /// \todo consolidate the angular momentum here - always in local frame? introduce physical quantity?
    /*if (storage.has(QuantityId::ANGULAR_VELOCITY)) {
        ArrayView<const Vector> omega = storage.getValue<Vector>(QuantityId::ANGULAR_VELOCITY);
        ArrayView<const SymmetricTensor> I = storage.getValue<SymmetricTensor>(QuantityId::MOMENT_OF_INERTIA);
        for (Size i = 0; i < v.size(); ++i) {
            total += I[i] * omega[i];
        }
    }*/

    SPH_ASSERT(isReal(total));
    return vectorCast<Float>(total);
}

TotalEnergy::TotalEnergy(const Float omega)
    : omega(0._f, 0._f, omega) {}

Float TotalEnergy::evaluate(const Storage& storage) const {
    double total = 0.;
    // add kinetic energy
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    for (Size i = 0; i < v.size(); ++i) {
        SPH_ASSERT(!v.empty());
        total += 0.5 * m[i] * getSqrLength(v[i]);
    }

    if (storage.has(QuantityId::ENERGY)) {
        // add internal energy
        ArrayView<const Float> u = storage.getValue<Float>(QuantityId::ENERGY);
        for (Size i = 0; i < v.size(); ++i) {
            SPH_ASSERT(!v.empty());
            total += m[i] * u[i];
        }
    }

    SPH_ASSERT(isReal(total));
    return Float(total);
}

TotalKineticEnergy::TotalKineticEnergy(const Float omega)
    : omega(0._f, 0._f, omega) {}

Float TotalKineticEnergy::evaluate(const Storage& storage) const {
    double total = 0.;
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    for (Size i = 0; i < v.size(); ++i) {
        SPH_ASSERT(!v.empty());
        total += 0.5 * m[i] * getSqrLength(v[i]);
    }
    SPH_ASSERT(isReal(total));
    return Float(total);
}

Float TotalInternalEnergy::evaluate(const Storage& storage) const {
    double total = 0.;
    if (!storage.has(QuantityId::ENERGY)) {
        return Float(total);
    }
    ArrayView<const Float> u = storage.getValue<Float>(QuantityId::ENERGY);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    SPH_ASSERT(!m.empty());
    for (Size i = 0; i < m.size(); ++i) {
        total += double(m[i] * u[i]);
    }
    SPH_ASSERT(isReal(total));
    return Float(total);
}

CenterOfMass::CenterOfMass(const Optional<Size> bodyId)
    : bodyId(bodyId) {}

Vector CenterOfMass::evaluate(const Storage& storage) const {
    Vector com(0._f);
    Float totalMass = 0._f;
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    auto accumulate = [&](const Size i) {
        totalMass += m[i];
        com += m[i] * r[i];
    };

    if (bodyId) {
        ArrayView<const Size> ids = storage.getValue<Size>(QuantityId::FLAG);
        for (Size i = 0; i < r.size(); ++i) {
            if (ids[i] == bodyId.value()) {
                accumulate(i);
            }
        }
    } else {
        for (Size i = 0; i < r.size(); ++i) {
            accumulate(i);
        }
    }
    return com / totalMass;
}

QuantityMeans::QuantityMeans(const QuantityId id, const Optional<Size> bodyId)
    : quantity(id)
    , bodyId(bodyId) {}

QuantityMeans::QuantityMeans(AutoPtr<IUserQuantity>&& func, const Optional<Size> bodyId)
    : quantity(std::move(func))
    , bodyId(bodyId) {}

MinMaxMean QuantityMeans::evaluate(const Storage& storage) const {
    MinMaxMean means;
    auto accumulate = [&](const auto& getter) {
        const Size size = storage.getParticleCnt();
        if (bodyId) {
            ArrayView<const Size> ids = storage.getValue<Size>(QuantityId::FLAG);
            for (Size i = 0; i < size; ++i) {
                if (ids[i] == bodyId.value()) {
                    means.accumulate(getter(i));
                }
            }
        } else {
            for (Size i = 0; i < size; ++i) {
                means.accumulate(getter(i));
            }
        }
    };
    if (quantity.getTypeIdx() == 0) {
        const QuantityId id = quantity.get<QuantityId>();
        SPH_ASSERT(storage.has(id));
        ArrayView<const Float> q = storage.getValue<Float>(id);
        accumulate([&](const Size i) { return q[i]; });
    } else {
        SPH_ASSERT(quantity.getTypeIdx() == 1);
        const AutoPtr<IUserQuantity>& func = quantity;
        func->initialize(storage);
        accumulate([&](const Size i) { return func->evaluate(i); });
    }
    return means;
}

QuantityValue::QuantityValue(const QuantityId id, const Size particleIdx)
    : id(id)
    , idx(particleIdx) {}

Float QuantityValue::evaluate(const Storage& storage) const {
    ArrayView<const Float> values = storage.getValue<Float>(id);
    return values[idx];
}

NAMESPACE_SPH_END
