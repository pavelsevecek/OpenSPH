#pragma once

#include "objects/containers/LookupMap.h"

NAMESPACE_SPH_BEGIN

class BarnesHut {
private:
    /// Source data
    ArrayView<const Vector> points;
    ArrayView<const Float> masses;

    /// Spatial grid
    LookupMap lut;

    /// Precomputed moments (so far just monopole)
    struct Monopole {
        Vector r;
        Float m;
    };
    Array<Monopole> monopoles;

    /// Opening angle for multipole approximation (in radians)
    Float cosTheta;

public:
    BarnesHut(const Float theta)
        : cosTheta(cos(theta)) {}

    void build(ArrayView<const Vector> points, ArrayView<const Float> masses) {
        this->points = points;
        this->masses = masses;
        ASSERT(points.size() == masses.size());
        if (lut.empty()) {
            const Size lutSize = root<3>(points.size()) + 1;
            lut = LookupMap(lutSize);
        }
        lut.update(points);

        const Size n = lut.getDimensionSize();
        monopoles.clear();
        for (Size x = 0; x < n; ++x) {
            for (Size y = 0; y < n; ++y) {
                for (Size z = 0; z < n; ++z) {
                    Array<Size>& idxs = lut(Indices(x, y, z));
                    if (idxs.empty()) {
                        monopoles.push(Monopole{ Vector(0._f), 0._f });
                    }
                    Float m = 0._f;
                    Vector r_com = Vector(0._f);
                    for (Size i : idxs) {
                        m += masses[i];
                        r_com += masses[i] * points[i];
                    }
                    r_com /= m;
                    monopoles.push(Monopole{ r_com, m });
                }
            }
        }
    }

    Vector evaluate(const Size index) const {
        const Vector r = points[index];
        const Float G = Constants::gravity;
        Vector f(0._f);
        Size i = 0;
        for (Size x = 0; x < n; ++x) {
            for (Size y = 0; y < n; ++y) {
                for (Size z = 0; z < n; ++z, ++i) {
                    const Indices idxs(x, y, z);
                    const Box box = lut.voxel(idxs);
                    const Float cosPhi = getLength(box.size()) / (getLength(r - box.center()) + EPS);
                    if (cosPhi < cosTheta) {
                        const Vector dr = monopoles[i].r - r;
                        f += G * monopoles[i].m * dr / pow<3>(getLength(r));
                    }
                }
            }
        }
    }
};


NAMESPACE_SPH_END
