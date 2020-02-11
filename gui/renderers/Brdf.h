#pragma once

#include "objects/geometry/Vector.h"

NAMESPACE_SPH_BEGIN

class IBrdf : public Polymorphic {
public:
    virtual Float transport(const Vector& normal, const Vector& dir_in, const Vector& dir_out) const = 0;
};

class LambertBrdf : public IBrdf {
private:
    Float albedo;

public:
    explicit LambertBrdf(const Float albedo)
        : albedo(albedo) {}

    virtual Float transport(const Vector& UNUSED(normal),
        const Vector& UNUSED(dir_in),
        const Vector& UNUSED(dir_out)) const override {
        return albedo / PI;
    }
};

class PhongBrdf : public IBrdf {
private:
    Float albedo;
    Float alpha = 2._f;

public:
    explicit PhongBrdf(const Float albedo)
        : albedo(albedo) {}

    virtual Float transport(const Vector& normal,
        const Vector& dir_in,
        const Vector& dir_out) const override {
        const Vector out = 2._f * dot(normal, dir_in) * normal - dir_in;
        return albedo / PI + 0.3_f * pow(abs(dot(out, dir_out)), alpha);
    }
};

struct HapkeParams {
    Float B0;
    Float h;
    Float g;
    Float theta_bar;
};

class HapkeBrdf : public IBrdf {
private:
    Float albedo;
    Float Aw;
    Float r0;
    Float B0;
    Float h;
    Float g;
    Float theta_bar;

public:
    virtual Float transport(const Vector& normal,
        const Vector& dir_in,
        const Vector& dir_out) const override {
        const Float mu_i = dot(normal, dir_in);
        const Float mu_e = dot(normal, dir_out);
        ASSERT(mu_i > 0._f && mu_e > 0._f, mu_i, mu_e);
        NOT_IMPLEMENTED;
        const Float alpha = 0._f;
        return albedo / (mu_i + mu_e) * ((1._f + B(alpha)) * P(alpha) + H(mu_i) * H(mu_e) - 1._f) *
               S(theta_bar);
    }

private:
    INLINE Float B(const Float alpha) const {
        return B0 / (1._f + tan(0.5_f * alpha) / h);
    }

    INLINE Float P(const Float alpha) const {
        return (1._f - sqr(g)) / pow(1._f + 2._f * g * cos(alpha) + sqr(g), 1.5_f);
    }

    INLINE Float H(const Float mu) const {
        return 1._f - Aw * mu / (r0 + 0.5_f * (1._f - 2._f * r0 * mu) * log((1._f + mu) / mu));
    }

    INLINE Float S(const Float theta) const {
        NOT_IMPLEMENTED;
        return theta;
    }
};

NAMESPACE_SPH_END
