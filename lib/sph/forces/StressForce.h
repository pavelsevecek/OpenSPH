#pragma once

#include "objects/wrappers/Flags.h"
#include "quantities/Material.h"
#include "quantities/Storage.h"
#include "solvers/Accumulator.h"
#include "solvers/Module.h"
#include "system/Logger.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

/// Object computing acceleration of particles and increase of internal energy due to divergence of the stress
/// tensor. When no stress tensor is used in the model, only pressure gradient is computed.
template <typename Yielding, typename Damage, typename AV>
class StressForce : public Module<Yielding, Damage, AV, RhoDivv, RhoGradv> {
private:
    RhoDivv rhoDivv;
    RhoGradv rhoGradv;
    ArrayView<Float> p, rho, du, u, m, cs;
    ArrayView<Vector> r, v, dv;
    ArrayView<TracelessTensor> s, ds;
    ArrayView<Size> bodyIdxs;
    ArrayView<Float> vonmises, D, q;

    enum class Options {
        USE_GRAD_P = 1 << 0,
        USE_DIV_S = 1 << 1,
    };
    Flags<Options> flags;

    Damage damage;
    Yielding yielding;
    AV av;

    FileLogger energy959;

    Array<Float> fp, fs, fav, rsum;

public:
    Statistics* stats;

    StressForce(const GlobalSettings& settings)
        : Module<Yielding, Damage, AV, RhoDivv, RhoGradv>(yielding, damage, av, rhoDivv, rhoGradv)
        , rhoGradv(QuantityIds::RHO_GRAD_V)
        , damage(settings)
        , av(settings)
        , energy959("energy959.txt") {
        flags.setIf(Options::USE_GRAD_P, settings.get<bool>(GlobalSettingsIds::MODEL_FORCE_GRAD_P));
        flags.setIf(Options::USE_DIV_S, settings.get<bool>(GlobalSettingsIds::MODEL_FORCE_DIV_S));
        // cannot use stress tensor without pressure
        ASSERT(!(flags.has(Options::USE_DIV_S) && !flags.has(Options::USE_GRAD_P)));
    }

    StressForce(const StressForce& other) = delete;

    StressForce(StressForce&& other) = default;

    void update(Storage& storage) {
        tie(rho, m) = storage.getValues<Float>(QuantityIds::DENSITY, QuantityIds::MASSES);
        tie(u, du) = storage.getAll<Float>(QuantityIds::ENERGY);
        tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
        if (flags.has(Options::USE_GRAD_P)) {
            p = storage.getValue<Float>(QuantityIds::PRESSURE);
            cs = storage.getValue<Float>(QuantityIds::SOUND_SPEED);
            // compute new values of pressure and sound speed
            EosAccessor eos(storage);
            for (Size i = 0; i < r.size(); ++i) {
                tieToTuple(p[i], cs[i]) = eos.evaluate(i);
            }
        }
        if (flags.has(Options::USE_DIV_S)) {
            tie(s, ds) = storage.getAll<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS);
        }
        bodyIdxs = storage.getValue<Size>(QuantityIds::FLAG);

        vonmises = storage.getValue<Float>(QuantityIds::YIELDING_REDUCE);
        q = storage.getValue<Float>(QuantityIds::HEATING);
        for (Size i = 0; i < q.size(); ++i) {
            q[i] = 0._f;
        }
        fp.resize(r.size());
        fs.resize(r.size());
        fav.resize(r.size());
        rsum.resize(r.size());

        fp.fill(0._f);
        fs.fill(0._f);
        fav.fill(0._f);
        rsum.fill(0._f);

        if (storage.has(QuantityIds::DAMAGE)) {
            D = storage.getValue<Float>(QuantityIds::DAMAGE);
        }
        damage.stats = stats;
        this->updateModules(storage);
    }

    INLINE void accumulate(const Size i, const Size j, const Vector& grad) {
        Vector f(0._f);
        // const Float rhoInvSqri = 1._f / sqr(rho[i]);
        // const Float rhoInvSqrj = 1._f / sqr(rho[j]);
        if (flags.has(Options::USE_GRAD_P)) {
            /// \todo measure if these branches have any effect on performance
            const auto avij = av(i, j);
            // f -= (reduce(p[i], i) * rhoInvSqri + reduce(p[j], i) * rhoInvSqrj + avij) * grad;
            f -= ((reduce(p[i], i) + reduce(p[j], j)) / (rho[i] * rho[j]) + avij) * grad;

            fp[i] -= m[j] * ((reduce(p[i], i) + reduce(p[j], j)) / (rho[i] * rho[j])) * grad[X];
            fp[j] += m[i] * ((reduce(p[i], i) + reduce(p[j], j)) / (rho[i] * rho[j])) * grad[X];

            fav[i] -= m[j] * avij * grad[X];
            fav[j] += m[i] * avij * grad[X];

            /*  if ((stats->get<Float>(StatisticsIds::TOTAL_TIME) > 0.0095) && (i == 145 || j == 145)) {
                  StdOutLogger logger;
                  logger.write("grad = ",
                      i,
                      "  ",
                      j,
                      "  ",
                      m[j] * ((reduce(p[i], i) + reduce(p[j], j)) / (rho[i] * rho[j])) * grad[X],
                      " ",
                      grad[X],
                      " ",
                      r[i][X] - r[j][X],
                      " ",
                      u[i],
                      " ",
                      u[j]);
              }*/
            // account for shock heating
            const Float heating = avij * dot(v[i] - v[j], grad);
            q[i] += m[j] * heating;
            q[j] += m[i] * heating;
        }
        Float redi = vonmises[i];
        Float redj = vonmises[j];
        if (D) {
            redi *= (1._f - pow<3>(D[i]));
            redj *= (1._f - pow<3>(D[j]));
        }
        if (flags.has(Options::USE_DIV_S) && bodyIdxs[i] == bodyIdxs[j] && redi != 0.f && redj != 0.f) {
            // apply stress only if particles belong to the same body
            /*   if (i == 1184 && j == 1350) {
                   StdOutLogger logger;
                   logger.write("div s  = ", (reduce(s[i], i) + reduce(s[j], j)) / (rho[i] * rho[j]));
               }*/
            f += (reduce(s[i], i) + reduce(s[j], j)) / (rho[i] * rho[j]) * grad;

            fs[i] += m[j] * ((reduce(s[i], i) + reduce(s[j], j)) / (rho[i] * rho[j]) * grad)[X];
            fs[j] -= m[i] * ((reduce(s[i], i) + reduce(s[j], j)) / (rho[i] * rho[j]) * grad)[X];
        }
        dv[i] += m[j] * f;
        dv[j] -= m[i] * f;

        const Float r2 = r[i][X] - r[j][X]; // getSqrLength(r[i] - r[j]);
        rsum[i] += m[j] * r2;
        rsum[j] -= m[i] * r2;
        // internal energy is computed at the end using accumulated values
        this->accumulateModules(i, j, grad);
    }

    void integrate(Storage& storage) {
        MaterialAccessor material(storage);
        for (Size i = 0; i < du.size(); ++i) {
            /// \todo check correct sign
            // const Float rhoInvSqr = 1._f / sqr(rho[i]);
            if (flags.has(Options::USE_GRAD_P)) {
                du[i] -= reduce(p[i], i) / rho[i] * rhoDivv[i];
            }
            du[i] += 0.5_f * q[i]; // heating
            if (flags.has(Options::USE_DIV_S)) {
                du[i] += 1._f / rho[i] * ddot(reduce(s[i], i), rhoGradv[i]);

                // compute derivatives of the stress tensor
                /// \todo rotation rate tensor?
                const Float mu = material.getParam<Float>(BodySettingsIds::SHEAR_MODULUS, i);
                /// \todo how to enforce that this expression is traceless tensor?
                ds[i] += TracelessTensor(
                    2._f * mu * (rhoGradv[i] - Tensor::identity() * rhoGradv[i].trace() / 3._f));

                /*if (i == 1450) {
                    StdOutLogger().write("ds(1450) = ", ds[i]);
                    StdOutLogger().write("stress(1450) = ", s[i]);
                }*/
                ASSERT(isReal(ds[i]));
            }
            ASSERT(isReal(du[i]));
        }
        /*const Float t = stats->get<Float>(StatisticsIds::TOTAL_TIME);
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityIds::POSITIONS);*/
        /*  const Size j = 959;
          energy959.write(
              t, "    ", u[j], "    ", du[j], "   ", r[j][H], "   ", reduce(p[j], j) / rho[j] * rhoDivv[j]);*/
        //"   ",
        // 1._f / rho[j] * ddot(reduce(s[j], j), rhoGradv[j]));

        /*   const Size j = 145;
           StdOutLogger logger;
           logger.write(stats->get<Float>(StatisticsIds::TOTAL_TIME),
               "  ",
               dv[j],
               " ",
               fp[j],
               " ",
               fav[j],
               " ",
               fs[j],
               " ",
               rho[j],
               " ",
               rsum[j],
               " ",
               u[j],
               " ",
               du[j],
               "  dv145");

           if ((stats->get<Float>(StatisticsIds::TOTAL_TIME) > 0.0095)) {
               exit(0);
           }*/
        this->integrateModules(storage);
    }

    void initialize(Storage& storage, const BodySettings& settings) const {
        storage.insert<Float, OrderEnum::FIRST_ORDER>(QuantityIds::ENERGY,
            settings.get<Float>(BodySettingsIds::ENERGY),
            settings.get<Range>(BodySettingsIds::ENERGY_RANGE));
        MaterialAccessor(storage).minimal(QuantityIds::ENERGY, 0) =
            settings.get<Float>(BodySettingsIds::ENERGY_MIN);
        if (flags.has(Options::USE_GRAD_P)) {
            // Compute pressure using equation of state
            std::unique_ptr<Abstract::Eos> eos = Factory::getEos(settings);
            const Float rho0 = settings.get<Float>(BodySettingsIds::DENSITY);
            const Float u0 = settings.get<Float>(BodySettingsIds::ENERGY);
            const Size n = storage.getParticleCnt();
            Array<Float> p(n), cs(n);
            for (Size i = 0; i < n; ++i) {
                tieToTuple(p[i], cs[i]) = eos->evaluate(rho0, u0);
            }
            storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::PRESSURE, std::move(p));
            storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::SOUND_SPEED, std::move(cs));
        }
        if (flags.has(Options::USE_DIV_S)) {
            storage.insert<TracelessTensor, OrderEnum::FIRST_ORDER>(QuantityIds::DEVIATORIC_STRESS,
                settings.get<TracelessTensor>(BodySettingsIds::STRESS_TENSOR),
                Range::unbounded());
            MaterialAccessor(storage).minimal(QuantityIds::DEVIATORIC_STRESS, 0) =
                settings.get<Float>(BodySettingsIds::STRESS_TENSOR_MIN);
            storage.insert<Tensor, OrderEnum::ZERO_ORDER>(QuantityIds::RHO_GRAD_V, Tensor::null());
            MaterialAccessor material(storage);
            material.setParams(BodySettingsIds::SHEAR_MODULUS, settings);
        }
        storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::HEATING, 0._f);

        this->initializeModules(storage, settings);
    }

private:
    /// \todo possibly precompute damage / yielding
    INLINE auto reduce(const Float pi, const Size idx) const {
        return damage.reduce(pi, idx);
    }

    INLINE auto reduce(const TracelessTensor& si, const Size idx) const {
        return damage.reduce(si, idx);
    }
};

NAMESPACE_SPH_END
