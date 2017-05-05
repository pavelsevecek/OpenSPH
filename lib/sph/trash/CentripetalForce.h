#pragma once

#include "solvers/Accumulator.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

/// Object computing acceleration of particles due to non-intertial reference frame.
class CentripetalForce : public Abstract::Force {
private:
    ArrayView<Vector> r, dv;
    Float omega;

public:
    CentripetalForce(const RunSettings& settings) {
        omega = settings.get<Float>(RunSettingsId::FRAME_ANGULAR_FREQUENCY);
    }

    INLINE virtual void integrate(Storage& storage) override {
        ArrayView<Vector> v;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
        // centripetal force is independent on particle relative position
        for (int i=0; i<r.size(); ++i) {
            const Float unitZ = Vector(0._f, 0._f, 1._f);
            dv[i] += omega * (r[i] - unitZ * dot(r[i], unitZ));
            dv[j] += omega * (r[j] - unitZ * dot(r[j], unitZ));
        }
    }
};

template<typename TPotential>
class ExternalPotential : public Abstract::Force {

};

NAMESPACE_SPH_END