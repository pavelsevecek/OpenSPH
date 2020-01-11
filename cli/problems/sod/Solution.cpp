#include "sod/Solution.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

static Pair<Float> computeCharacteristics(const SodConfig& sod,
    const Float P3,
    const Float P,
    const Float c) {
    const Float u = P3 / P;
    if (u > 1) {
        const Float term1 = sod.gamma * ((sod.gamma + 1.) * u + sod.gamma - 1.);
        const Float term2 = sqrt(2. / term1);
        const Float fp = (u - 1.) * c * term2;
        const Float dfdp =
            c * term2 / P + (u - 1.) * c / term2 * (-1. / sqr(term1)) * sod.gamma * (sod.gamma + 1.) / P;
        return { fp, dfdp };
    } else {
        const Float beta = (sod.gamma - 1) / (2 * sod.gamma);
        const Float fp = (pow(u, beta) - 1.) * (2. * c / (sod.gamma - 1.));
        const Float dfdp = 2. * c / (sod.gamma - 1.) * beta * pow(u, (beta - 1.)) / P;
        return { fp, dfdp };
    }
}

static Pair<Float> RiemannProblem(const SodConfig& sod) {
    const Float c_l = sqrt(sod.gamma * sod.P_l / sod.rho_l);
    const Float c_r = sqrt(sod.gamma * sod.P_r / sod.rho_r);

    const Float beta = (sod.gamma - 1) / (2 * sod.gamma);

    Float P_new = pow((c_l + c_r + (sod.u_l - sod.u_r) * 0.5 * (sod.gamma - 1.)) /
                          (c_l / pow(sod.P_l, beta) + c_r / pow(sod.P_r, beta)),
        1. / beta);
    Float P3 = 0.5 * (sod.P_r + sod.P_l);
    Float f_L, f_R, dfdp_L, dfdp_R;
    while (fabs(P3 - P_new) > 1e-6) {
        P3 = P_new;
        tie(f_L, dfdp_L) = computeCharacteristics(sod, P3, sod.P_l, c_l);
        tie(f_R, dfdp_R) = computeCharacteristics(sod, P3, sod.P_r, c_r);
        const Float f = f_L + f_R + (sod.u_r - sod.u_l);
        const Float df = dfdp_L + dfdp_R;
        const Float dp = -f / df;
        P_new = P3 + dp;
    }
    const Float v_3 = sod.u_l - f_L;
    return { P_new, v_3 };
}

Storage analyticSod(const SodConfig& sod, const Float t) {
    const Float x_min = -0.5;
    const Float x_max = 0.5;
    const Float mu = sqrt((sod.gamma - 1._f) / (sod.gamma + 1._f));
    // const Float beta = (sod.gamma - 1) / (2 * sod.gamma);

    /// Sound speed
    const Float c_l = sqrt(sod.gamma * sod.P_l / sod.rho_l);

    Float P_post;
    Float v_post;
    tie(P_post, v_post) = RiemannProblem(sod);

    const Float rho_post = sod.rho_r * (((P_post / sod.P_r) + sqr(mu)) / (1 + mu * mu * (P_post / sod.P_r)));
    const Float v_shock = v_post * ((rho_post / sod.rho_r) / ((rho_post / sod.rho_r) - 1));
    const Float rho_middle = sod.rho_l * pow((P_post / sod.P_l), 1 / sod.gamma);

    // Key Positions
    const Float x1 = sod.x0 - c_l * t;
    const Float x3 = sod.x0 + v_post * t;
    const Float x4 = sod.x0 + v_shock * t;

    // determining x2
    const Float c_2 = c_l - ((sod.gamma - 1) / 2) * v_post;
    const Float x2 = sod.x0 + (v_post - c_2) * t;
    // start setting values
    Array<Vector> pos(1000);
    for (Size i = 0; i < pos.size(); ++i) {
        const Float x = x_min + (x_max - x_min) / pos.size() * i;
        pos[i] = Vector(x, 0._f, 0._f, EPS);
    }
    Storage storage;
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::FIRST, std::move(pos));
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, 0._f);
    storage.insert<Float>(QuantityId::PRESSURE, OrderEnum::ZERO, 0._f);
    storage.insert<Float>(QuantityId::ENERGY, OrderEnum::ZERO, 1._f);

    ArrayView<Vector> r, v;
    tie(r, v) = storage.getAll<Vector>(QuantityId::POSITION);
    ArrayView<Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
    ArrayView<Float> P = storage.getValue<Float>(QuantityId::PRESSURE);
    ArrayView<Float> u = storage.getValue<Float>(QuantityId::ENERGY);

    for (Size i = 0; i < r.size(); ++i) {
        const Float x = r[i][X];
        if (x < x1) {
            // Solution left of x1
            rho[i] = sod.rho_l;
            P[i] = sod.P_l;
            v[i][X] = sod.u_l;
        } else if (x1 <= x && x <= x2) {
            // Solution between x1 and x2
            const Float c = mu * mu * ((sod.x0 - x) / t) + (1 - mu * mu) * c_l;
            rho[i] = sod.rho_l * pow((c / c_l), 2 / (sod.gamma - 1));
            P[i] = sod.P_l * pow((rho[i] / sod.rho_l), sod.gamma);
            v[i][X] = (1 - mu * mu) * ((-(sod.x0 - x) / t) + c_l);
        } else if (x2 <= x && x <= x3) {
            // Solution between x2 and x3
            rho[i] = rho_middle;
            P[i] = P_post;
            v[i][X] = v_post;
        } else if (x3 <= x && x <= x4) {
            //  Solution between x3 and x4
            rho[i] = rho_post;
            P[i] = P_post;
            v[i][X] = v_post;
        } else {
            // Solution after x4
            rho[i] = sod.rho_r;
            P[i] = sod.P_r;
            v[i][X] = sod.u_r;
        }
        u[i] = P[i] / ((sod.gamma - 1._f) * rho[i]);
    }
    return storage;
}


NAMESPACE_SPH_END
