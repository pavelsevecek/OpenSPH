#pragma once

#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

/// \brief Single point-mass particle.
class Attractor {
    Vector r;
    Vector v;
    Vector dv;
    Float rad;
    Float m;

    // BodySettings body = EMPTY_SETTINGS;

public:
    Attractor() = default;

    Attractor(const Vector& pos, const Vector& velocity, const Float radius, const Float mass) {
        r = pos;
        v = velocity;
        dv = Vector(0._f);
        rad = radius;
        m = mass;
    }

    Float mass() const {
        return m;
    }

    Float radius() const {
        return rad;
    }

    const Vector& position() const {
        return r;
    }

    Vector& position() {
        return r;
    }

    const Vector& velocity() const {
        return v;
    }

    Vector& velocity() {
        return v;
    }

    const Vector& acceleration() const {
        return dv;
    }

    Vector& acceleration() {
        return dv;
    }

    /*   const BodySettings& params() const {
           return body;
       }

       BodySettings& params() {
           return body;
       }*/
};

NAMESPACE_SPH_END
