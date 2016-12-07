#pragma once


#include "storage/Storage.h"

NAMESPACE_SPH_BEGIN

/// Forces acting on SPH particles.

/// Forces must implement method dv, du, setQuantities and update. They are passed to solver using
/// template parameter to avoid calling virtual functions in inner loop of the solver.





template <typename TArtificialViscosity>
struct PressureForce : public Noncopyable {
    TArtificialViscosity av;
    ArrayView<const Float> p, rho;
    ArrayView<const Vector> v;


    PressureForce(const Settings<GlobalSettingsIds>& settings)
        : av(settings) {}

    static void setQuantities(Storage& storage, const Settings<BodySettingsIds>& settings) {
        // here pressure, density and velocity must be already set by the solver, just check
        ASSERT(storage.has(QuantityKey::RHO) && storage.has(QuantityKey::P) && storage.has(QuantityKey::R));
        // set quantities needed by artificial viscosity
        TArtificialViscosity::setQuantities(storage, settings);
    }

    void update(Storage& storage) {
        tieToArray(p, rho) = storage.getValues<Float>(QuantityKey::P, QuantityKey::RHO);
        v = storage.getAll<Vector>(QuantityKey::R)[1];
        av.update(storage);
    }

    INLINE Vector dv(const int i, const int j, const Vector& grad) {
        /// \todo generalize for tensor AV, using specialization
        return -(p[i] / Math::sqr(rho[i]) + p[j] / Math::sqr(rho[j]) + av(i, j)) * grad;
    }

    INLINE Float du(const int i, const int j, const Vector& grad) {
        const Float divv = dot(v[i] - v[j], grad);
        return -divv * (p[i] / Math::sqr(rho[i]) + 0.5_f * av(i, j));
    }
};

/*
template <typename TArtificialViscosity>
struct StressForce : public PressureForce<TArtificialViscosity> {
    ArrayView<const TracelessTensor> s;

    void setQuantities(Storage& storage, const Settings<BodySettingsIds>& settings) {
        PressureForce<TArtificialViscosity>::setQuantities(storage, settings);
        if (!storage.has(QuantityKey::S)) {
            TracelessTensor s0 = settings.get<TracelessTensor>(BodySettingsIds::STRESS_TENSOR);
            storage.emplace<TracelessTensor, OrderEnum::FIRST_ORDER>(QuantityKey::S, s0);
        }
    }

    void update(Storage& storage) {
        tieToArray(p, rho) = storage.getValues<Float>(QuantityKey::P, QuantityKey::RHO);
        ArrayView<Vector> r, dv;
        /// \todo here is one place where getting derivative directly from storage would be helpful
        tieToArray(r, v, dv) = storage.getAll<Vector>(QuantityKey::RHO);
        av.update(storage);
    }

    INLINE Float operator()(const Vector grad) {
        // artificial viscosity is already accounted for in the pressure term
        return pressureTerm(grad) + (s[i] / Math::sqr(rho[i]) + s[j] / Math::sqr(rho[j])).apply(grad);
    }

    INLINE Float energy(const Vector grad) {
        return pressureTerm(i, j, grad) + 1._f / rho[i] * ddot(s[i], edot[i]);
    }
};*/

NAMESPACE_SPH_END
