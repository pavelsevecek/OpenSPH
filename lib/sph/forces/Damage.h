#pragma once

#include "geometry/Vector.h"
#include "objects/containers/ArrayView.h"
#include "storage/QuantityMap.h"

NAMESPACE_SPH_BEGIN

class DummyDamage : public Object {
public:
    void update(Storage& UNUSED(storage)) {}

    INLINE Float operator()(const Float p, const int UNUSED(i)) const { return p; }

    INLINE TracelessTensor operator()(const TracelessTensor& s, const int UNUSED(i)) const { return s; }

    void integrate(Storage& UNUSED(storage)) {}

    QuantityMap getMap() { return {}; }
};

/// Scalar damage describing fragmentation of the body according to Grady-Kipp model (Grady and Kipp, 1980)
class ScalarDamage : public Object {
private:
    // here d actually contains third root of damage
    ArrayView<Float> damage;

public:
    void update(Storage& storage) { damage = storage.getValue<Float>(QuantityKey::D); }

    /// Reduce pressure
    INLINE Float operator()(const Float p, const int i) const {
        const Float d = Math::pow<3>(damage[i]);
        if (p < 0._f) {
            return (1._f - d) * p;
        } else {
            return p;
        }
    }

    /// Reduce deviatoric stress tensor
    INLINE TracelessTensor operator()(const TracelessTensor& s, const int i) const {
        const Float d = Math::pow<3>(damage[i]);
        return (1._f - d) * s;
    }

    void integrate(Storage& UNUSED(storage)) { NOT_IMPLEMENTED; }

    QuantityMap getMap() const {
        QuantityMap map;
        map[QuantityKey::D] = { ValueEnum::SCALAR, OrderEnum::FIRST_ORDER };
        return map;
    }
};

NAMESPACE_SPH_END
