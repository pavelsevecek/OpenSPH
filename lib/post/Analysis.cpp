#include "post/Analysis.h"
#include "io/Column.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "objects/finders/BruteForceFinder.h"
#include "objects/finders/UniformGrid.h"
#include "objects/geometry/Box.h"
#include "objects/utility/Iterators.h"
#include "quantities/Storage.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

Size Post::findComponents(const Storage& storage,
    const Float radius,
    const ComponentConnectivity connectivity,
    Array<Size>& indices) {
    ASSERT(radius > 0._f);
    // get values from storage
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);

    AutoPtr<IBasicFinder> finder = makeAuto<UniformGridFinder>();
    Float actRadius = radius;

    // Checks if two particles belong to the same component
    struct IComponentChecker : public Polymorphic {
        virtual bool belong(const Size i, const Size j) = 0;
    };

    AutoPtr<IComponentChecker> checker;

    switch (connectivity) {
    case ComponentConnectivity::SEPARATE_BY_FLAG:
        class FlagComponentChecker : public IComponentChecker {
            ArrayView<const Size> flag;

        public:
            explicit FlagComponentChecker(const Storage& storage) {
                flag = storage.getValue<Size>(QuantityId::FLAG);
            }
            virtual bool belong(const Size i, const Size j) override {
                return flag[i] == flag[j];
            }
        };
        checker = makeAuto<FlagComponentChecker>(storage);
        break;
    case ComponentConnectivity::OVERLAP:
        class AnyChecker : public IComponentChecker {
        public:
            virtual bool belong(const Size UNUSED(i), const Size UNUSED(j)) override {
                // any two particles within the search radius belong to the same component
                return true;
            }
        };
        checker = makeAuto<AnyChecker>();
        break;
    case ComponentConnectivity::ESCAPE_VELOCITY:
        class EscapeVelocityComponentChecker : public IComponentChecker {
            ArrayView<const Vector> r;
            ArrayView<const Vector> v;
            ArrayView<const Float> m;
            Float radius;

        public:
            explicit EscapeVelocityComponentChecker(const Storage& storage, const Float radius)
                : radius(radius) {
                r = storage.getValue<Vector>(QuantityId::POSITION);
                v = storage.getDt<Vector>(QuantityId::POSITION);
                m = storage.getValue<Float>(QuantityId::MASS);
            }
            virtual bool belong(const Size i, const Size j) override {
                const Float dv = getLength(v[i] - v[j]);
                const Float dr = getLength(r[i] - r[j]);
                const Float m_tot = m[i] + m[j];
                const Float v_esc = sqrt(2._f * Constants::gravity * m_tot / dr);
                return dr < 0.5_f * radius * (r[i][H] + r[j][H]) || dv < v_esc;
            }
        };
        checker = makeAuto<EscapeVelocityComponentChecker>(storage, radius);
        actRadius = 10._f * radius;
        break;
    }

    ASSERT(finder && checker);

    // initialize stuff
    indices.resize(r.size());
    const Size unassigned = Size(-1);
    indices.fill(unassigned);
    Size componentIdx = 0;
    finder->build(r);

    Array<Size> stack;
    Array<NeighbourRecord> neighs;

    for (Size i = 0; i < r.size(); ++i) {
        if (indices[i] == unassigned) {
            indices[i] = componentIdx;
            stack.push(i);
            // find new neigbours recursively until we find all particles in the component
            while (!stack.empty()) {
                const Size index = stack.pop();
                finder->findAll(index, r[index][H] * actRadius, neighs);
                for (auto& n : neighs) {
                    if (!checker->belong(index, n.index)) {
                        // do not count as neighbours
                        continue;
                    }
                    if (indices[n.index] == unassigned) {
                        indices[n.index] = componentIdx;
                        stack.push(n.index);
                    }
                }
            }
            componentIdx++;
        }
    }
    return componentIdx;
}

static Storage clone(const Storage& storage) {
    Storage cloned;
    const Array<Vector>& r = storage.getValue<Vector>(QuantityId::POSITION);
    cloned.insert<Vector>(QuantityId::POSITION, OrderEnum::FIRST, r.clone());

    const Array<Vector>& v = storage.getDt<Vector>(QuantityId::POSITION);
    cloned.getDt<Vector>(QuantityId::POSITION) = v.clone();

    if (storage.has(QuantityId::MASS)) {
        const Array<Float>& m = storage.getValue<Float>(QuantityId::MASS);
        cloned.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, m.clone());
    } else {
        ArrayView<const Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
        Float rhoAvg = 0._f;
        for (Size i = 0; i < r.size(); ++i) {
            rhoAvg += rho[i];
        }
        rhoAvg /= r.size();

        /// \todo ASSUMING 10km body!
        const Float m = sphereVolume(5.e3_f) * rhoAvg / r.size();
        cloned.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, m);
    }

    return cloned;
}

Storage Post::findFutureBodies2(const Storage& storage, ILogger& logger) {
    Array<Vector> r = storage.getValue<Vector>(QuantityId::POSITION).clone();
    Array<Vector> v = storage.getDt<Vector>(QuantityId::POSITION).clone();
    const Float m = sphereVolume(5.e3_f) * 2700.f / r.size();
    Float W_tot = 0._f;
    for (Size i = 0; i < r.size(); ++i) {
        for (Size j = i + 1; j < r.size(); ++j) {
            W_tot += Constants::gravity * sqr(m) / getLength(r[i] - r[j]);
            ASSERT(isReal(W_tot));
        }
    }

    Size iteration = 0;
    while (true) {
        // find velocity of COM
        Vector v0(0._f);
        for (Size i = 0; i < v.size(); ++i) {
            v0 += v[i];
        }
        v0 /= v.size();

        // find kinetic energies
        Float K_tot = 0._f;
        Float K_largest = 0._f;
        Size idx_largest = 0;
        for (Size i = 0; i < r.size(); ++i) {
            const Float k = 0.5_f * m * getSqrLength(v[i] - v0);
            K_tot += k;
            if (k > K_largest) {
                K_largest = k;
                idx_largest = i;
            }
        }

        logger.write("Iteration ", iteration++, ", W = ", W_tot, " / K = ", K_tot);
        if (K_tot > W_tot) {
            for (Size i = 0; i < r.size(); ++i) {
                if (i != idx_largest) {
                    W_tot -= Constants::gravity * sqr(m) / getLength(r[i] - r[idx_largest]);
                }
                ASSERT(W_tot > 0._f);
            }

            r.remove(idx_largest);
            v.remove(idx_largest);
        } else {
            break;
        }
    }

    logger.write("Find largest remnant with ", r.size(), " particles");
    return clone(storage);
}

Storage Post::findFutureBodies(const Storage& storage, const Float particleRadius, ILogger& logger) {
    Storage cloned = clone(storage);
    Size numComponents = 0, prevNumComponents;
    Size iter = 0;
    do {
        Array<Size> indices;
        prevNumComponents = numComponents;

        logger.write(
            "Iteration ", iter, ": number of bodies: ", iter == 0 ? storage.getParticleCnt() : numComponents);

        // do merging the first iteration, the follow with energy considerations
        ComponentConnectivity connectivity =
            (iter == 0) ? ComponentConnectivity::OVERLAP : ComponentConnectivity::ESCAPE_VELOCITY;
        numComponents = findComponents(cloned, particleRadius, connectivity, indices);

        Array<Vector> r_new(numComponents);
        Array<Vector> v_new(numComponents);
        Array<Float> h_new(numComponents);
        Array<Float> m_new(numComponents);
        r_new.fill(Vector(0._f));
        v_new.fill(Vector(0._f));
        h_new.fill(0._f);
        m_new.fill(0._f);


        ArrayView<const Vector> r = cloned.getValue<Vector>(QuantityId::POSITION);
        ArrayView<const Vector> v = cloned.getDt<Vector>(QuantityId::POSITION);
        ArrayView<const Float> m = cloned.getValue<Float>(QuantityId::MASS);

        for (Size i = 0; i < r.size(); ++i) {
            m_new[indices[i]] += m[i];
            r_new[indices[i]] += m[i] * r[i];
            h_new[indices[i]] += pow<3>(r[i][H]);
            v_new[indices[i]] += m[i] * v[i];
        }
        for (Size i = 0; i < numComponents; ++i) {
            ASSERT(m_new[i] != 0._f);
            r_new[i] /= m_new[i];
            r_new[i][H] = root<3>(h_new[i]);
            v_new[i] /= m_new[i];
        }

        cloned.getValue<Vector>(QuantityId::POSITION) = std::move(r_new);
        cloned.getDt<Vector>(QuantityId::POSITION) = std::move(v_new);
        cloned.getValue<Float>(QuantityId::MASS) = std::move(m_new);

        iter++;
    } while (numComponents != prevNumComponents);

    return cloned;
}

Array<Post::MoonEnum> Post::findMoons(const Storage& storage, const Float radius, const Float limit) {
    // first, find the larget one
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    Size largestIdx = 0;
    Float largestM = 0._f;
    for (Size i = 0; i < m.size(); ++i) {
        if (m[i] > largestM) {
            largestM = m[i];
            largestIdx = i;
        }
    }

    Array<MoonEnum> status(m.size());
#ifdef SPH_DEBUG
    status.fill(MoonEnum(-1));
#endif
    status[largestIdx] = MoonEnum::LARGEST_FRAGMENT;

    // find the ellipse for all bodies
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < m.size(); ++i) {
        if (i == largestIdx) {
            continue;
        }

        // check for observability
        if (r[i][H] < limit * r[largestIdx][H]) {
            status[i] = MoonEnum::UNOBSERVABLE;
            continue;
        }

        // compute the orbital elements
        Optional<KeplerianElements> elements = findKeplerEllipse(
            m[i] + largestM, m[i] * largestM / (m[i] + largestM), r[i] - r[largestIdx], v[i] - v[largestIdx]);

        if (!elements) {
            // not bound, mark as ejected body
            status[i] = MoonEnum::RUNAWAY;
        } else {
            // if the pericenter is closer than the sum of radii, mark as impactor
            if (elements->pericenterDist() < radius * (r[i][H] + r[largestIdx][H])) {
                status[i] = MoonEnum::IMPACTOR;
            } else {
                // bound and not on colli
                status[i] = MoonEnum::MOON;
            }
        }
    }

    return status;
}

Array<Post::Tumbler> Post::findTumblers(const Storage& storage, const Float limit) {
    Array<Tumbler> tumblers;
    ArrayView<const Vector> omega = storage.getValue<Vector>(QuantityId::ANGULAR_VELOCITY);
    ArrayView<const SymmetricTensor> I = storage.getValue<SymmetricTensor>(QuantityId::MOMENT_OF_INERTIA);

    for (Size i = 0; i < omega.size(); ++i) {
        const Vector L = I[i] * omega[i];
        if (omega[i] == Vector(0._f)) {
            continue;
        }
        const Float cosBeta = dot(L, omega[i]) / (getLength(L) * getLength(omega[i]));
        ASSERT(cosBeta >= -1._f && cosBeta <= 1._f);
        const Float beta = acos(cosBeta);
        if (beta > limit) {
            tumblers.push(Tumbler{ i, beta });
        }
    }
    return tumblers;
}

SymmetricTensor Post::getInertiaTensor(ArrayView<const Float> m,
    ArrayView<const Vector> r,
    const Vector& r0) {
    TODO("test!");
    SymmetricTensor I = SymmetricTensor::null();
    for (Size i = 0; i < r.size(); ++i) {
        const Vector dr = r[i] - r0;
        I += m[i] * (SymmetricTensor::identity() * getSqrLength(dr) - outer(dr, dr));
    }
    return I;
}

SymmetricTensor Post::getInertiaTensor(ArrayView<const Float> m, ArrayView<const Vector> r) {
    Vector com(0._f);
    Float m_tot = 0._f;
    for (Size i = 0; i < r.size(); ++i) {
        com += m[i] * r[i];
        m_tot += m[i];
    }
    com /= m_tot;

    return getInertiaTensor(m, r, com);
}

Float Post::KeplerianElements::ascendingNode() const {
    if (sqr(L[Z]) > (1._f - EPS) * getSqrLength(L)) {
        // Longitude of the ascending node undefined, return 0 (this is a valid case, not an error the data)
        return 0._f;
    } else {
        return -atan2(L[X], L[Y]);
    }
}

Float Post::KeplerianElements::periapsisArgument() const {
    if (e < EPS) {
        return 0._f;
    }
    const Float Omega = this->ascendingNode();
    const Vector OmegaDir(cos(Omega), sin(Omega), 0._f); // direction of the ascending node
    const Float omega = acos(dot(OmegaDir, getNormalized(K)));
    if (K[Z] < 0._f) {
        return 2._f * PI - omega;
    } else {
        return omega;
    }
}

Float Post::KeplerianElements::pericenterDist() const {
    return a * (1._f - e);
}

Float Post::KeplerianElements::semiminorAxis() const {
    return a * sqrt(1._f - e * e);
}

Optional<Post::KeplerianElements> Post::findKeplerEllipse(const Float M,
    const Float mu,
    const Vector& r,
    const Vector& v) {
    const Float E = 0.5_f * mu * getSqrLength(v) - Constants::gravity * M * mu / getLength(r);
    if (E >= 0._f) {
        // parabolic or hyperbolic trajectory
        return NOTHING;
    }

    // http://sirrah.troja.mff.cuni.cz/~davok/scripta-NB1.pdf
    KeplerianElements elements;
    elements.a = -Constants::gravity * mu * M / (2._f * E);

    const Vector L = mu * cross(r, v); // angular momentum
    ASSERT(L != Vector(0._f));
    elements.i = acos(L[Z] / getLength(L));
    elements.e = sqrt(1._f + 2._f * E * getSqrLength(L) / (sqr(Constants::gravity) * pow<3>(mu) * sqr(M)));

    elements.K = cross(v, L) - Constants::gravity * mu * M * getNormalized(r);
    elements.L = L;

    return elements;
}


static Array<Float> getBodiesRadii(const Storage& storage,
    const Post::HistogramParams& params,
    const Post::HistogramId id) {
    switch (params.source) {
    case Post::HistogramParams::Source::PARTICLES: {
        Array<Float> values(storage.getParticleCnt());
        switch (id) {
        case Post::HistogramId::RADII: {
            ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
            for (Size i = 0; i < r.size(); ++i) {
                values[i] = r[i][H];
            }
            break;
        }
        case Post::HistogramId::EQUIVALENT_MASS_RADII: {
            ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
            for (Size i = 0; i < m.size(); ++i) {
                values[i] = root<3>(3.f * m[i] / (params.referenceDensity * 4.f * PI));
            }
            break;
        }
        case Post::HistogramId::VELOCITIES: {
            ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
            for (Size i = 0; i < v.size(); ++i) {
                values[i] = getLength(v[i]);
            }
            break;
        }
        case Post::HistogramId::ROTATIONAL_PERIOD: {
            if (!storage.has(QuantityId::ANGULAR_VELOCITY)) {
                /// \todo should be generalized for all quantities
                values.fill(0._f);
                break;
            }
            values.clear();
            ArrayView<const Vector> omega = storage.getValue<Vector>(QuantityId::ANGULAR_VELOCITY);
            for (Size i = 0; i < omega.size(); ++i) {
                const Float w = getLength(omega[i]);
                if (w > 0._f) {
                    values.push(2._f * PI / (3600._f * w));
                }
            }
            break;
        }
        default:
            const QuantityId quantityId = QuantityId(id);
            ASSERT((int)quantityId >= 0);
            /// \todo allow also other types
            values = storage.getValue<Float>(quantityId).clone();
            break;
        }
        std::sort(values.begin(), values.end());
        return values;
    }
    case Post::HistogramParams::Source::COMPONENTS: {
        Array<Size> components;
        const Size numComponents = findComponents(
            storage, params.components.radius, Post::ComponentConnectivity::OVERLAP, components);

        Array<Float> values(numComponents);
        values.fill(0._f);
        switch (id) {
        case Post::HistogramId::RADII: {
            // compute volume of the body
            ArrayView<const Float> rho, m;
            tie(rho, m) = storage.getValues<Float>(QuantityId::DENSITY, QuantityId::MASS);
            for (Size i = 0; i < rho.size(); ++i) {
                values[components[i]] += m[i] / rho[i];
            }

            // compute equivalent radii from volumes
            Array<Float> radii(numComponents);
            for (Size i = 0; i < numComponents; ++i) {
                radii[i] = root<3>(values[i]);
                ASSERT(isReal(radii[i]) && radii[i] > 0._f, radii[i]);
            }
            std::sort(radii.begin(), radii.end());
            return radii;
        }
        case Post::HistogramId::VELOCITIES: {
            // compute velocity as weighted average
            ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
            ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
            Array<Vector> sumV(numComponents);
            Array<Float> weights(numComponents);
            sumV.fill(Vector(0._f));
            weights.fill(0._f);
            for (Size i = 0; i < m.size(); ++i) {
                const Size componentIdx = components[i];
                sumV[componentIdx] += m[i] * v[i];
                weights[componentIdx] += m[i];
            }
            for (Size i = 0; i < numComponents; ++i) {
                ASSERT(weights[i] != 0._f);
                values[i] = getLength(sumV[i] / weights[i]);
            }
            std::sort(values.begin(), values.end());
            return values;
        }
        default:
            NOT_IMPLEMENTED;
        }
    }
    default:
        NOT_IMPLEMENTED;
    }
}

Array<Post::SfdPoint> Post::getCummulativeSfd(const Storage& storage, const Post::HistogramParams& params) {
    Array<Float> radii = getBodiesRadii(storage, params, params.id);

    Interval range = params.range;
    if (range.empty()) {
        for (Float r : radii) {
            if (params.validator(r)) {
                range.extend(r);
            }
        }
    }
    ASSERT(!range.empty());

    Array<SfdPoint> histogram;
    Size count = 1;
    Float lastR = INFTY;
    // iterate in reverse order - from largest radii to smallest ones
    for (Float r : reverse(radii)) {
        if (r < lastR) {
            if (range.contains(r) && params.validator(r)) {
                histogram.push(SfdPoint{ r, count });
            }
            lastR = r;
        }
        count++;
    }
    ASSERT(histogram.size() > 0);

    return histogram;
}

Array<Post::SfdPoint> Post::getDifferentialSfd(const Storage& storage, const HistogramParams& params) {
    Array<Float> radii = getBodiesRadii(storage, params, params.id);

    Interval range = params.range;
    if (range.empty()) {
        for (Float r : radii) {
            if (params.validator(r)) {
                range.extend(r);
            }
        }
    }
    ASSERT(!range.empty());

    Size binCnt = params.binCnt;
    if (binCnt == 0) {
        // estimate initial bin count as sqrt of component count
        binCnt = Size(sqrt(radii.size()));
    }

    Array<Size> sfd(binCnt);
    sfd.fill(0);
    // check for case where only one body/particle exists
    const bool singular = range.size() == 0;
    for (Float r : radii) {
        if (!params.validator(r)) {
            continue;
        }
        // get bin index
        Size binIdx;
        if (singular) {
            binIdx = 0; // just add everything into the first bin to get some reasonable output
        } else {
            const Float floatIdx = binCnt * (r - range.lower()) / range.size();
            if (floatIdx >= 0._f && floatIdx < binCnt) {
                binIdx = Size(floatIdx);
            } else {
                // out of range, skip
                continue;
            }
        }
        sfd[binIdx]++;
    }
    // convert to SfdPoints
    Array<SfdPoint> histogram(binCnt);
    for (Size i = 0; i < binCnt; ++i) {
        histogram[i] = { range.lower() + (i * range.size()) / binCnt, sfd[i] };
        ASSERT(isReal(histogram[i].value));
    }
    return histogram;
}

NAMESPACE_SPH_END
