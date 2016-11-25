#pragma once

#include "math/Math.h"
#include "objects/containers/Array.h"


NAMESPACE_SPH_BEGIN

template<typename TFunctor>
INLINE Float findRoot(const Float initial, TFunctor&& functor) {

}

/// Analytical solutions of the Sod shock tube
/// http://www.phys.lsu.edu/~tohline/PHYS7412/sod.html
inline void analyticSod(const float t) {
    /// Initial conditions
    const Float x0    = 0._f;
    const Float rho_l = 1._f;
    const Float P_l   = 1._f;
    const Float u_l   = 0._f;

    const Float rho_r = 0.125_f;
    const Float P_r   = 0.1_f;
    const Float u_r   = 0._f;

    const Float gamma = 1.4._f;
    const Float mu    = Math::sqrt((gamma - 1._f) / (gamma + 1._f));

    /// Sound speed
    const Float c_l = Math::sqrt((gamma * P_l / rho_l));
    const Float c_r = Math::sqrt((gamma * P_r / rho_r));

    auto func =
        [=](const Float P) {
            return (P - P_r) *
                       (Math::sqrt((Math::sqr(1._f - mu * mu)) * (1._f / (rho_r * (P + mu * mu * P_r))))) -
                   2._f * (Math::sqrt(gamma) / (gamma - 1._f)) *
                       (1._f - Math::pow(P, (gamma - 1) / (2._f * gamma)));
        };

   /* const Float P_post = fzero('sod_func', pi);
    v_post             = 2 * (sqrt(gamma) / (gamma - 1)) * (1 - power(P_post, (gamma - 1) / (2 * gamma)));
    rho_post           = rho_r * (((P_post / P_r) + mu ^ 2) / (1 + mu * mu * (P_post / P_r)));
    v_shock            = v_post * ((rho_post / rho_r) / ((rho_post / rho_r) - 1));
    rho_middle         = (rho_l)*power((P_post / P_l), 1 / gamma);

    % Key Positions x1   = x0 - c_l * t;
    x3                   = x0 + v_post * t;
    x4                   = x0 + v_shock * t;
    % determining x2 c_2 = c_l - ((gamma - 1) / 2) * v_post;
    x2                   = x0 + (v_post - c_2) * t;

    % start setting values n_points              = 1000;
    % set by user % boundaries(can be set) x_min = -0.5;
    x_max                                        = 0.5;

    x      = linspace(x_min, x_max, n_points);
    data.x = x'; data.rho = zeros(n_points, 1); % density data.P = zeros(n_points, 1);
    % pressure data.u                                            = zeros(n_points, 1);
    % velocity data.e                                            = zeros(n_points, 1); %internal energy

for index = 1:n_points
    if data.x(index) < x1
        %Solution b4 x1
        data.rho(index) = rho_l;
    data.P(index)       = P_l;
    data.u(index)       = u_l;
    elseif(x1 <= data.x(index) && data.x(index) <= x2) % Solution b / w x1 and x2 c =
        mu * mu * ((x0 - data.x(index)) / t) + (1 - mu * mu) * c_l;
    data.rho(index) = rho_l * power((c / c_l), 2 / (gamma - 1));
    data.P(index)   = P_l * power((data.rho(index) / rho_l), gamma);
    data.u(index)   = (1 - mu * mu) * ((-(x0 - data.x(index)) / t) + c_l);
    elseif(x2 <= data.x(index) && data.x(index) <= x3) % Solution b / w x2 and x3 data.rho(index) =
        rho_middle;
    data.P(index)                                                                                 = P_post;
    data.u(index)                                                                                 = v_post;
    elseif(x3 <= data.x(index) && data.x(index) <= x4) % Solution b / w x3 and x4 data.rho(index) = rho_post;
    data.P(index)                                                                                 = P_post;
    data.u(index)                                                                                 = v_post;
    elseif x4 < data.x(index) % Solution after x4 data.rho(index)                                 = rho_r;
    data.P(index)                                                                                 = P_r;
    data.u(index)                                                                                 = u_r;
    end data.e(index) = data.P(index) / ((gamma - 1) * data.rho(index));
    end*/
}


NAMESPACE_SPH_END
