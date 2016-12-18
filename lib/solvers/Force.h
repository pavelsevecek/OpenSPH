#pragma once

#include "objects/wrappers/Flags.h"
#include "solvers/Accumulator.h"
#include "storage/Storage.h"

NAMESPACE_SPH_BEGIN

/// Object computing acceleration of particles and increase of internal energy due to divergence of the stress
/// tensor. When no stress tensor is used in the model, only pressure gradient is computed.
class StressForce : public Noncopyable {
private:
    Accumulator<RhoDivv> rhoDivv;
    Accumulator<RhoGradv> rhoGradv;
    ArrayView<Float> p, rho, du, u, m;
    ArrayView<Vector> v, dv;
    ArrayView<TracelessTensor> s;

    enum class Options {
        USE_GRAD_P = 1 << 0,
        USE_DIV_S = 1 << 1,
    };
    Flags<Options> flags;


public:
    StressForce(const Settings<GlobalSettingsIds>& settings) {
        flags.setIf(Options::USE_GRAD_P, settings.get<bool>(GlobalSettingsIds::MODEL_FORCE_GRAD_P));
        flags.setIf(Options::USE_DIV_S, settings.get<bool>(GlobalSettingsIds::MODEL_FORCE_DIV_S));
        // cannot use stress tensor without pressure
        ASSERT(!(flags.has(Options::USE_DIV_S) && !flags.has(Options::USE_GRAD_P)));
    }

    void update(Storage& storage) {
        tieToArray(rho, u, m) = storage.getValues<Float>(QuantityKey::RHO, QuantityKey::U, QuantityKey::M);
        ArrayView<const Vector> r;
        tieToArray(r, v, dv) = storage.getAll<Vector>(QuantityKey::R);
        if (flags.has(Options::USE_GRAD_P)) {
            p = storage.getValue<Float>(QuantityKey::P);
        }
        if (flags.has(Options::USE_DIV_S)) {
            s = storage.getValue<TracelessTensor>(QuantityKey::S);
        }
        rhoDivv.update(storage);
        rhoGradv.update(storage);
    }

    INLINE void accumulate(const int i, const int j, const Vector& grad) {
        Vector f(0._f);
        const Float rhoInvSqri = 1._f / Math::sqr(rho[i]);
        const Float rhoInvSqrj = 1._f / Math::sqr(rho[j]);
        if (flags.has(Options::USE_GRAD_P)) {
            /// \todo measure if these branches have any effect on performance
            f -= (p[i] * rhoInvSqri + p[j] * rhoInvSqrj) * grad;
            rhoDivv.accumulate(i, j, grad);
        }
        if (flags.has(Options::USE_DIV_S)) {
            f += (s[i] * rhoInvSqri + s[j] * rhoInvSqrj) * grad;
            rhoGradv.accumulate(i, j, grad);
        }
        dv[i] += m[j] * f;
        dv[j] -= m[i] * f;
        // internal energy is computed at the end using accumulated values
    }

    void evaluate(Storage&) {
        for (int i = 0; i < du.size(); ++i) {
            ASSERT(du[i] == 0._f); // derivative must be cleared
            /// \todo check correct sign
            const Float rhoInvSqr = 1._f / Math::sqr(rho[i]);
            if (flags.has(Options::USE_GRAD_P)) {
                du[i] -= p[i] * rhoInvSqr * rhoDivv[i];
            }
            if (flags.has(Options::USE_DIV_S)) {
                du[i] += rhoInvSqr * ddot(s[i], rhoGradv[i]);
            }
        }
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

    stress evolution equation
    ds = 2._f * shearModulus * (gradv  - Tensor::identity()*gradv.trace()/3._f);
};*/

/*
 struct CentripetalForce {
    INLINE Vector dv(const int i, const int j, const Vector& grad) {
    // centripetal force is independent on particle relative position, acts on both particles
        return omega * (r[i] - Vector(0._f, 0._f, 1._f)*dot(r[i], Vector(0._f, 0._f, 1._f)));
    }*/

NAMESPACE_SPH_END
