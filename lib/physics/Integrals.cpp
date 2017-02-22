#include "physics/Integrals.h"
#include "post/Components.h"
#include "quantities/Storage.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN


Float TotalMass::evaluate(Storage& storage) const {
    Float total(0._f);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityIds::MASSES);
    ASSERT(!m.empty());
    for (Size i = 0; i < m.size(); ++i) {
        total += m[i];
    }
    ASSERT(isReal(total));
    return total;
}

TotalMomentum::TotalMomentum(const Float omega)
    : omega(0._f, 0._f, omega) {}


Vector TotalMomentum::evaluate(Storage& storage) const {
    BasicVector<double> total(0.); // compute in double precision to avoid round-off error during accumulation
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityIds::MASSES);
    ASSERT(!v.empty());
    for (Size i = 0; i < v.size(); ++i) {
        total += vectorCast<double>(m[i] * (v[i] + cross(omega, r[i])));
    }
    ASSERT(isReal(total));
    return vectorCast<Float>(total);
}

TotalAngularMomentum::TotalAngularMomentum(const Float omega)
    : omega(0._f, 0._f, omega) {}

Vector TotalAngularMomentum::evaluate(Storage& storage) const {
    BasicVector<double> total(0.);
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityIds::MASSES);
    ASSERT(!v.empty());
    for (Size i = 0; i < v.size(); ++i) {
        total += vectorCast<double>(m[i] * cross(r[i], (v[i] + cross(omega, r[i]))));
    }
    ASSERT(isReal(total));
    return vectorCast<Float>(total);
}

TotalEnergy::TotalEnergy(const Float omega)
    : omega(0._f, 0._f, omega) {}

Float TotalEnergy::evaluate(Storage& storage) const {
    double total = 0.;
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityIds::MASSES);
    ArrayView<const Float> u = storage.getValue<Float>(QuantityIds::ENERGY);
    for (Size i = 0; i < v.size(); ++i) {
        ASSERT(!v.empty());
        total += 0.5 * m[i] * getSqrLength(v[i]) + m[i] * u[i];
    }
    ASSERT(isReal(total));
    return Float(total);
}

TotalKineticEnergy::TotalKineticEnergy(const Float omega)
    : omega(0._f, 0._f, omega) {}

Float TotalKineticEnergy::evaluate(Storage& storage) const {
    double total = 0.;
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityIds::MASSES);
    for (Size i = 0; i < v.size(); ++i) {
        ASSERT(!v.empty());
        total += 0.5 * m[i] * getSqrLength(v[i]);
    }
    ASSERT(isReal(total));
    return Float(total);
}

Float TotalInternalEnergy::evaluate(Storage& storage) const {
    double total = 0.;
    ArrayView<const Float> u = storage.getValue<Float>(QuantityIds::ENERGY);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityIds::MASSES);
    ASSERT(!m.empty());
    for (Size i = 0; i < m.size(); ++i) {
        total += double(m[i] * u[i]);
    }
    ASSERT(isReal(total));
    return Float(total);
}

CenterOfMass::CenterOfMass(const Optional<Size> bodyId)
    : bodyId(bodyId) {}

Vector CenterOfMass::evaluate(Storage& storage) const {
    Vector com(0._f);
    Float totalMass = 0._f;
    ArrayView<const Float> m = storage.getValue<Float>(QuantityIds::MASSES);
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityIds::POSITIONS);
    auto accumulate = [&](const Size i) {
        totalMass += m[i];
        com += m[i] * r[i];
    };

    if (bodyId) {
        ArrayView<const Size> ids = storage.getValue<Size>(QuantityIds::FLAG);
        for (Size i = 0; i < r.size(); ++i) {
            if (ids[i] == bodyId.get()) {
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

QuantityMeans::QuantityMeans(const QuantityIds id, const Optional<Size> bodyId)
    : quantity(id)
    , bodyId(bodyId) {}

QuantityMeans::QuantityMeans(const std::function<Float(const Size i)>& func, const Optional<Size> bodyId)
    : quantity(func)
    , bodyId(bodyId) {}

Means QuantityMeans::evaluate(Storage& storage) const {
    Means means;
    auto accumulate = [&](const auto& getter) {
        const Size size = storage.getParticleCnt();
        if (bodyId) {
            ArrayView<const Size> ids = storage.getValue<Size>(QuantityIds::FLAG);
            for (Size i = 0; i < size; ++i) {
                if (ids[i] == bodyId.get()) {
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
        const QuantityIds id = quantity.get<QuantityIds>();
        ASSERT(storage.has(id));
        ArrayView<const Float> q = storage.getValue<Float>(id);
        accumulate([&](const Size i) { return q[i]; });
    } else {
        ASSERT(quantity.getTypeIdx() == 1);
        accumulate(quantity.get<std::function<Float(const Size i)>>());
    }
    return means;
}

NAMESPACE_SPH_END
