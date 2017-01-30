#include "physics/Integrals.h"
#include "post/Components.h"
#include "quantities/Storage.h"
#include "system/Factory.h"

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

Vector Integrals::getCenterOfMass(Storage& storage, const Optional<Size> bodyId) const {
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


Diagnostics::Diagnostics(const GlobalSettings& settings)
    : settings(settings) {}


Size Diagnostics::getNumberOfComponents(Storage& storage) const {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityIds::POSITIONS);
    Array<Size> indices;
    return findComponents(r, settings, indices);
}

Array<Diagnostics::Pair> Diagnostics::getParticlePairs(Storage& storage, const Float limit) const {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityIds::POSITIONS);
    std::unique_ptr<Abstract::Finder> finder = Factory::getFinder(settings);
    finder->build(r);
    const Float radius = Factory::getKernel<3>(settings).radius();

    Array<Diagnostics::Pair> pairs;
    Array<NeighbourRecord> neighs;

    /// \todo symmetrized h
    for (Size i = 0; i < r.size(); ++i) {
        // only smaller h to go through each pair only once
        finder->findNeighbours(i, r[i][H] * radius, neighs, FinderFlags::FIND_ONLY_SMALLER_H);
        for (auto& n : neighs) {
            if (getSqrLength(r[i] - r[n.index]) < sqr(limit * r[i][H])) {
                pairs.push(Diagnostics::Pair{ i, n.index });
            }
        }
    }
    return pairs;
}

NAMESPACE_SPH_END
