#pragma once

/// \file BruteForceGravity.h
/// \brief Simple gravity solver evaluating all particle pairs
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gravity/AbstractGravity.h"
#include "physics/Constants.h"

NAMESPACE_SPH_BEGIN

/// \brief Computes gravitational acceleration by summing up forces from all particle pairs.
///
/// This implementation is not intended for high-performance code because of the O(N) complexity. Useful for
/// testing and debugging purposes.
class BruteForceGravity : public Abstract::Gravity {
private:
    ArrayView<const Vector> r;
    ArrayView<const Float> m;

public:
    virtual void build(ArrayView<const Vector> points, ArrayView<const Float> masses) override {
        r = points;
        m = masses;
    }

    virtual Vector eval(const Size idx) override {
        ASSERT(r && m);
        Vector a(0._f);
        for (Size i = 0; i < r.size(); ++i) {
            if (i == idx) {
                continue;
            }
            const Vector dr = r[i] - r[idx];
            a += m[i] * dr / pow<3>(getLength(dr) + EPS);
        }
        return Constants::gravity * a;
    }
};

NAMESPACE_SPH_END
