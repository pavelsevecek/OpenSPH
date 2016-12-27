#pragma once

#include "geometry/Vector.h"
#include "objects/containers/ArrayView.h"
#include "system/Settings.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

class DummyDamage : public Object {
public:
    DummyDamage(const GlobalSettings& UNUSED(settings),
        const std::function<TracelessTensor(const TracelessTensor&, const int)>&) {}

    INLINE Float reduce(const Float p, const int UNUSED(i)) const { return p; }

    INLINE TracelessTensor reduce(const TracelessTensor& s, const int UNUSED(i)) const { return s; }
};


enum class ExplicitFlaws {
    UNIFORM,  ///< Distribute flaws uniformly (to random particles), see Benz & Asphaug (1994), Sec. 3.3.1
    ASSIGNED, ///< Explicitly assigned flaws by user, used mainly for testing purposes. Values must be set in
              /// quantity
};

/// Scalar damage describing fragmentation of the body according to Grady-Kipp model (Grady and Kipp, 1980)
class ScalarDamage : public Object {
private:
    // here d actually contains third root of damage
    ArrayView<Float> damage;
    Float kernelRadius;

    using Yielding = std::function<TracelessTensor(const TracelessTensor&, const int)>;
    Yielding yielding;

    ExplicitFlaws options;

public:
    ScalarDamage(const GlobalSettings& settings,
        const Yielding& yielding,
        const ExplicitFlaws options = ExplicitFlaws::UNIFORM);

    void initialize(Storage& storage, const BodySettings& settings) const;

    void update(Storage& storage) { damage = storage.getValue<Float>(QuantityKey::DAMAGE); }

    void integrate(Storage& storage);


    /// Reduce pressure
    INLINE Float reduce(const Float p, const int i) const {
        const Float d = Math::pow<3>(damage[i]);
        if (p < 0._f) {
            return (1._f - d) * p;
        } else {
            return p;
        }
    }

    /// Reduce deviatoric stress tensor
    INLINE TracelessTensor reduce(const TracelessTensor& s, const int i) const {
        const Float d = Math::pow<3>(damage[i]);
        return (1._f - d) * s;
    }
};

NAMESPACE_SPH_END
