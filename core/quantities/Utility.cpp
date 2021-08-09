#include "quantities/Utility.h"
#include "objects/finders/NeighborFinder.h"
#include "objects/utility/Algorithm.h"
#include "system/Factory.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

Float getTotalMass(const Storage& storage) {
    Float m_tot = 0._f;
    if (!storage.empty()) {
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        m_tot += accumulate(m, 0._f);
    }
    for (const Attractor& p : storage.getAttractors()) {
        m_tot += p.mass();
    }
    return m_tot;
}

Vector getTotalMomentum(const Storage& storage) {
    Vector p_tot(0._f);
    if (!storage.empty()) {
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
        for (Size i = 0; i < v.size(); ++i) {
            p_tot += m[i] * v[i];
        }
    }
    for (const Attractor& p : storage.getAttractors()) {
        p_tot += p.mass() * p.velocity();
    }
    return p_tot;
}

void moveInertialFrame(Storage& storage, const Vector positionOffset, const Vector velocityOffset) {
    transform(
        storage,
        [&positionOffset](const Vector& r) { return r + positionOffset; },
        [&velocityOffset](const Vector& v) { return v + velocityOffset; });
}

Vector moveToCenterOfMassFrame(ArrayView<const Float> m, ArrayView<Vector> r) {
    SPH_ASSERT(m.size() == r.size());
    Vector r_com(0._f);
    Float m_tot = 0._f;
    for (Size i = 0; i < r.size(); ++i) {
        r_com += m[i] * r[i];
        m_tot += m[i];
    }
    // Dangerous! Do not modify smoothing length!
    r_com = clearH(r_com / m_tot);
    for (Size i = 0; i < r.size(); ++i) {
        r[i] -= r_com;
    }
    return r_com;
}

void moveToCenterOfMassFrame(Storage& storage) {
    ArrayView<const Float> m;
    ArrayView<Vector> r, v;
    if (!storage.empty()) {
        m = storage.getValue<Float>(QuantityId::MASS);
        r = storage.getValue<Vector>(QuantityId::POSITION);
        v = storage.getDt<Vector>(QuantityId::POSITION);
    }
    ArrayView<Attractor> pm = storage.getAttractors();

    Vector r_com(0._f);
    Vector v_com(0._f);
    Float m_tot = 0._f;
    for (Size i = 0; i < r.size(); ++i) {
        r_com += m[i] * r[i];
        v_com += m[i] * v[i];
        m_tot += m[i];
    }
    for (const Attractor& p : pm) {
        r_com += p.mass() * p.position();
        v_com += p.mass() * p.velocity();
        m_tot += p.mass();
    }
    SPH_ASSERT(m_tot > 0._f, m_tot);

    // Dangerous! Do not modify smoothing length!
    r_com = clearH(r_com / m_tot);
    v_com = clearH(v_com / m_tot);

    for (Size i = 0; i < r.size(); ++i) {
        r[i] -= r_com;
        v[i] -= v_com;
    }
    for (Attractor& p : pm) {
        p.position() -= r_com;
        p.velocity() -= v_com;
    }
}


NAMESPACE_SPH_END