#include "sph/initial/Distribution.h"
#include "math/Integrator.h"
#include "math/Morton.h"
#include "math/rng/VectorRng.h"
#include "objects/finders/UniformGrid.h"
#include "objects/geometry/Domain.h"
#include "objects/wrappers/Optional.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// RandomDistribution implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

RandomDistribution::RandomDistribution(const Size seed)
    : rng(seed) {}

Array<Vector> RandomDistribution::generate(const Size n, const IDomain& domain) const {
    const Box bounds = domain.getBoundingBox();
    VectorRng<UniformRng&> boxRng(rng);
    Array<Vector> vecs(0, n);
    // use homogeneous smoothing lenghs regardless of actual spatial variability of particle concentration
    const Float volume = domain.getVolume();
    const Float h = root<3>(volume / n);
    Size found = 0;
    for (Size i = 0; i < 1e5 * n && found < n; ++i) {
        Vector w = boxRng() * bounds.size() + bounds.lower();
        w[H] = h;
        if (domain.contains(w)) {
            vecs.push(w);
            ++found;
        }
    }
    return vecs;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CubicPacking implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

Array<Vector> CubicPacking::generate(const Size n, const IDomain& domain) const {
    PROFILE_SCOPE("CubicPacking::generate")
    ASSERT(n > 0);
    const Float volume = domain.getVolume();
    const Float particleDensity = Float(n) / volume;

    // interparticle distance based on density
    const Float h = 1._f / root<3>(particleDensity);
    ASSERT(isReal(h));

    const Box boundingBox = domain.getBoundingBox();
    const Vector step(h);
    const Box box(boundingBox.lower() + 0.5_f * step, boundingBox.upper());
    Array<Vector> vecs;
    box.iterate(step, [&vecs, &domain, h](Vector&& v) {
        if (domain.contains(v)) {
            v[H] = h;
            vecs.push(std::move(v));
        }
    });
    return vecs;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// HexagonalPacking implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

HexagonalPacking::HexagonalPacking(const Flags<Options> f)
    : flags(f) {}

Array<Vector> HexagonalPacking::generate(const Size n, const IDomain& domain) const {
    PROFILE_SCOPE("HexagonalPacking::generate")
    ASSERT(n > 0);
    const Float volume = domain.getVolume();
    const Float particleDensity = Float(n) / volume;

    // interparticle distance based on density
    const Float h = 1._f / root<3>(particleDensity);
    const Float dx = (flags.has(Options::SPH5_COMPATIBILITY) ? 1._f : 1.1_f) * h;
    const Float dy = sqrt(3._f) * 0.5_f * dx;
    const Float dz = sqrt(6._f) / 3._f * dx;

    const Box boundingBox = domain.getBoundingBox();
    const Vector step(dx, dy, dz);
    const Box box = flags.has(Options::SPH5_COMPATIBILITY)
                        ? boundingBox
                        : Box(boundingBox.lower() + 0.5_f * step, boundingBox.upper());
    Array<Vector> vecs;
    const Float deltaX = 0.5_f * dx;
    const Float deltaY = sqrt(3._f) / 6._f * dx;
    box.iterateWithIndices(step, [&](Indices&& idxs, Vector&& v) {
        if (idxs[2] % 2 == 0) {
            if (idxs[1] % 2 == 1) {
                v[X] += deltaX;
            }
        } else {
            if (idxs[1] % 2 == 0) {
                v[X] += deltaX;
            }
            v[Y] += deltaY;
        }
        if (domain.contains(v)) {
            v[H] = h;
            vecs.push(std::move(v));
        }
    });
    if (flags.has(Options::SORTED)) {
        // sort by Morton code
        std::sort(vecs.begin(), vecs.end(), [&box](Vector& v1, Vector& v2) {
            // compute relative coordinates in bounding box
            const Vector vr1 = (v1 - box.lower()) / box.size();
            const Vector vr2 = (v2 - box.lower()) / box.size();
            return morton(vr1) > morton(vr2);
        });
    }
    if (flags.has(Options::CENTER)) {
        ASSERT(!vecs.empty());
        Vector com(0._f);
        for (const Vector& v : vecs) {
            com += v;
        }
        com /= vecs.size();
        // match center of mass to center of domain
        const Vector delta = domain.getCenter() - com;
        for (Vector& v : vecs) {
            const Float h = v[H];
            v += delta;
            v[H] = h;
        }
    }
    return vecs;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DiehlEtAlDistribution implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

DiehlDistribution::DiehlDistribution(const DiehlDistribution::DensityFunc& particleDensity,
    const Float error,
    const Size numOfIters,
    const Float strength,
    const Float small)
    : particleDensity(particleDensity)
    , error(error)
    , numOfIters(numOfIters)
    , strength(strength)
    , small(small) {}

namespace {
    class ForwardingDomain : public IDomain {
    private:
        const IDomain& domain;

    public:
        explicit ForwardingDomain(const IDomain& domain)
            : IDomain(domain.getCenter())
            , domain(domain) {}

        virtual Box getBoundingBox() const override {
            return domain.getBoundingBox();
        }

        virtual Float getVolume() const override {
            return domain.getVolume();
        }

        virtual bool contains(const Vector& v) const override {
            return domain.contains(v);
        }

        virtual void getSubset(ArrayView<const Vector> vs,
            Array<Size>& output,
            const SubsetType type) const override {
            return domain.getSubset(vs, output, type);
        }

        virtual void getDistanceToBoundary(ArrayView<const Vector> vs,
            Array<Float>& distances) const override {
            domain.getDistanceToBoundary(vs, distances);
        }

        virtual void project(ArrayView<Vector> vs,
            Optional<ArrayView<Size>> indices = NOTHING) const override {
            domain.project(vs, indices);
        }
        virtual void addGhosts(ArrayView<const Vector> vs,
            Array<Ghost>& ghosts,
            const Float radius = 2._f,
            const Float eps = 0.05_f) const override {
            domain.addGhosts(vs, ghosts, radius, eps);
        }
    };
} // namespace

Array<Vector> DiehlDistribution::generate(const Size n, const IDomain& domain) const {
    // Renormalize particle density so that integral matches expected particle count
    Float multiplier = 1._f;
    auto actDensity = [this, &multiplier](const Vector& v) { return multiplier * particleDensity(v); };

    Integrator<HaltonQrng> mc(makeAuto<ForwardingDomain>(domain));
    Size cnt = 0;
    Float particleCnt;
    for (particleCnt = mc.integrate(actDensity); abs(particleCnt - n) > error;) {
        const Float ratio = n / max(particleCnt, 1._f);
        multiplier *= ratio;
        particleCnt = mc.integrate(actDensity);
        if (cnt++ > 100) { // break potential infinite loop
            break;
        }
    }
    const Size N = Size(particleCnt); // final particle count of the target

    Box boundingBox = domain.getBoundingBox();
    VectorPdfRng<HaltonQrng> rng(boundingBox, actDensity);

    // generate initial particle positions
    Array<Vector> vecs;
    for (Size i = 0; i < N; ++i) {
        Vector v = rng();
        const Float n = actDensity(v);
        const Float h = 1._f / root<3>(n);
        v[H] = h;
        vecs.push(v);
    }

    UniformGridFinder finder;
    Array<NeighbourRecord> neighs;
    finder.build(vecs);

    const Float correction = strength / (1.f + small);
    // radius of search, does not have to be equal to radius of used SPH kernel
    const Float kernelRadius = 2._f;

    for (Size k = 0; k < numOfIters; ++k) {
        // gradually decrease the strength of particle dislocation
        const Float converg = 1._f / sqrt(Float(k + 1));

        // reconstruct finder to allow for variable topology of particles
        finder.rebuild();

        // for all particles ...
        for (Size i = 0; i < N; ++i) {
            Vector delta(0._f);
            const Float n = actDensity(vecs[i]); // average particle density
            // average interparticle distance at given point
            const Float neighbourRadius = kernelRadius / root<3>(n);
            finder.findNeighbours(i, neighbourRadius, neighs, EMPTY_FLAGS, 1.e-3_f);

            /// \todo find a way to add ghosts
            // create ghosts for particles near the boundary
            // Vector<float, d> mirror;
            // if (vecs[i].getSqrLength() > radius - neighbourRadius) {
            //    mirror = vecs[i].sphericalInversion(Vector<float, d>(0.f), radius);
            //    neighbours.push(&mirror);
            //}
            for (Size j = 0; j < neighs.size(); ++j) {
                const Size k = neighs[j].index;
                const Vector diff = vecs[k] - vecs[i];
                const Float lengthSqr = getSqrLength(diff);
                // average kernel radius to allow for the gradient of particle density
                const Float h = kernelRadius *
                                (0.5f / root<3>(actDensity(vecs[i])) + 0.5f / root<3>(actDensity(vecs[k])));
                if (lengthSqr > h * h || lengthSqr == 0) {
                    continue;
                }
                const Float hSqrInv = 1.f / (h * h);
                const Float length = getLength(diff);
                ASSERT(length != 0._f);
                const Vector diffUnit = diff / length;
                const Float t =
                    converg * h * (strength / (small + getSqrLength(diff) * hSqrInv) - correction);
                delta += diffUnit * min(t, h); // clamp the dislocation to particle distance
            }
            vecs[i] = vecs[i] - delta;
        }
        // project particles outside of the domain to the boundary
        domain.project(vecs);
    }

    return vecs;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// LinearDistribution implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

Array<Vector> LinearDistribution::generate(const Size n, const IDomain& domain) const {
    const Float center = domain.getCenter()[X];
    const Float radius = 0.5_f * domain.getBoundingBox().size()[X];
    Array<Vector> vs(0, n);
    const Float dx = 2._f * radius / (n - 1);
    for (Size i = 0; i < n; ++i) {
        const Float x = center - radius + (2._f * radius * i) / (n - 1);
        vs.push(Vector(x, 0._f, 0._f, 1.5_f * dx)); // smoothing length = interparticle distance
    }
    return vs;
}

NAMESPACE_SPH_END
