#include "physics/Integrals.h"

NAMESPACE_SPH_BEGIN

Integrals::Integrals() = default;

Integrals::Integrals(const GlobalSettings& settings) {
    const Float freq = settings.get<Float>(GlobalSettingsIds::FRAME_ANGULAR_FREQUENCY);
    omega = Vector(0._f, 0._f, 1._f) * freq;
}

Float Integrals::getTotalMass(Storage& storage) const {
    Float total(0._f);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityIds::MASSES);
    ASSERT(!m.empty());
    for (Size i = 0; i < m.size(); ++i) {
        total += m[i];
    }
    ASSERT(isReal(total));
    return total;
}

Vector Integrals::getTotalMomentum(Storage& storage) const {
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

Vector Integrals::getTotalAngularMomentum(Storage& storage) const {
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

Float Integrals::getTotalEnergy(Storage& storage) const {
    return getTotalKineticEnergy(storage) + getTotalInternalEnergy(storage);
}

Float Integrals::getTotalKineticEnergy(Storage& storage) const {
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

Float Integrals::getTotalInternalEnergy(Storage& storage) const {
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

NAMESPACE_SPH_END
