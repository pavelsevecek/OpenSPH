#include "physics/Integrals.h"
#include "post/Analysis.h"
#include "quantities/Storage.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN


Float TotalMass::evaluate(const Storage& storage) const {
    Float total(0._f);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASSES);
    ASSERT(!m.empty());
    for (Size i = 0; i < m.size(); ++i) {
        total += m[i];
    }
    ASSERT(isReal(total));
    return total;
}

TotalMomentum::TotalMomentum(const Float omega)
    : omega(0._f, 0._f, omega) {}


Vector TotalMomentum::evaluate(const Storage& storage) const {
    BasicVector<double> total(0.); // compute in double precision to avoid round-off error during accumulation
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASSES);
    ASSERT(!v.empty());
    for (Size i = 0; i < v.size(); ++i) {
        total += vectorCast<double>(m[i] * (v[i] + cross(omega, r[i])));
    }
    ASSERT(isReal(total));
    return vectorCast<Float>(total);
}

TotalAngularMomentum::TotalAngularMomentum(const Float omega)
    : frameFrequency(0._f, 0._f, omega) {}

Vector TotalAngularMomentum::evaluate(const Storage& storage) const {
    BasicVector<double> total(0.);
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASSES);

    // angular momentum with respect to origin
    ASSERT(!v.empty());
    for (Size i = 0; i < v.size(); ++i) {
        total += vectorCast<double>(m[i] * cross(r[i], (v[i] + cross(frameFrequency, r[i]))));
    }

    // local angular momentum
    if (storage.has(QuantityId::ANGULAR_VELOCITY)) {
        ArrayView<const Vector> omega = storage.getValue<Vector>(QuantityId::ANGULAR_VELOCITY);
        ArrayView<const Float> I = storage.getValue<Float>(QuantityId::MOMENT_OF_INERTIA);
        for (Size i = 0; i < v.size(); ++i) {
            total += I[i] * omega[i];
        }
    }

    ASSERT(isReal(total));
    return vectorCast<Float>(total);
}

TotalEnergy::TotalEnergy(const Float omega)
    : omega(0._f, 0._f, omega) {}

Float TotalEnergy::evaluate(const Storage& storage) const {
    double total = 0.;
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASSES);
    ArrayView<const Float> u = storage.getValue<Float>(QuantityId::ENERGY);
    for (Size i = 0; i < v.size(); ++i) {
        ASSERT(!v.empty());
        total += 0.5 * m[i] * getSqrLength(v[i]) + m[i] * u[i];
    }
    ASSERT(isReal(total));
    return Float(total);
}

TotalKineticEnergy::TotalKineticEnergy(const Float omega)
    : omega(0._f, 0._f, omega) {}

Float TotalKineticEnergy::evaluate(const Storage& storage) const {
    double total = 0.;
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASSES);
    for (Size i = 0; i < v.size(); ++i) {
        ASSERT(!v.empty());
        total += 0.5 * m[i] * getSqrLength(v[i]);
    }
    ASSERT(isReal(total));
    return Float(total);
}

Float TotalInternalEnergy::evaluate(const Storage& storage) const {
    double total = 0.;
    ArrayView<const Float> u = storage.getValue<Float>(QuantityId::ENERGY);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASSES);
    ASSERT(!m.empty());
    for (Size i = 0; i < m.size(); ++i) {
        total += double(m[i] * u[i]);
    }
    ASSERT(isReal(total));
    return Float(total);
}

CenterOfMass::CenterOfMass(const Optional<Size> bodyId)
    : bodyId(bodyId) {}

Vector CenterOfMass::evaluate(const Storage& storage) const {
    Vector com(0._f);
    Float totalMass = 0._f;
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASSES);
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
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
        ASSERT(storage.has(id));
        ArrayView<const Float> q = storage.getValue<Float>(id);
        accumulate([&](const Size i) { return q[i]; });
    } else {
        ASSERT(quantity.getTypeIdx() == 1);
        const AutoPtr<IUserQuantity>& func = quantity;
        func->initialize(storage);
        accumulate([&](const Size i) { return func->evaluate(i); });
    }
    return means;
}

NAMESPACE_SPH_END
