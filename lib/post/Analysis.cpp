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
#include "thread/Scheduler.h"
#include <numeric>

NAMESPACE_SPH_BEGIN

Array<Size> Post::findNeighbourCounts(const Storage& storage, const Float particleRadius) {
    Array<NeighbourRecord> neighs;
    AutoPtr<IBasicFinder> finder = Factory::getFinder(RunSettings::getDefaults());
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    finder->build(SEQUENTIAL, r);
    Array<Size> counts(r.size());
    for (Size i = 0; i < r.size(); ++i) {
        counts[i] = finder->findAll(i, r[i][H] * particleRadius, neighs);
    }
    return counts;
}

// Checks if two particles belong to the same component
struct IComponentChecker : public Polymorphic {
    virtual bool belong(const Size i, const Size j) = 0;
};

static Size findComponentsImpl(IComponentChecker& checker,
    ArrayView<const Vector> r,
    const Float radius,
    Array<Size>& indices) {
    // initialize stuff
    indices.resize(r.size());
    const Size unassigned = Size(-1);
    indices.fill(unassigned);
    Size componentIdx = 0;

    Array<Size> stack;
    Array<NeighbourRecord> neighs;

    AutoPtr<IBasicFinder> finder = Factory::getFinder(RunSettings::getDefaults());
    // the build time is probably negligible compared to the actual search of components, so let's just use
    // the sequential execution here
    finder->build(SEQUENTIAL, r);

    for (Size i = 0; i < r.size(); ++i) {
        if (indices[i] == unassigned) {
            indices[i] = componentIdx;
            stack.push(i);
            // find new neigbours recursively until we find all particles in the component
            while (!stack.empty()) {
                const Size index = stack.pop();
                finder->findAll(index, r[index][H] * radius, neighs);
                for (auto& n : neighs) {
                    if (!checker.belong(index, n.index)) {
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

Size Post::findComponents(const Storage& storage,
    const Float radius,
    const ComponentConnectivity connectivity,
    Array<Size>& indices) {
    ASSERT(radius > 0._f);

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
    case ComponentConnectivity::ESCAPE_VELOCITY:
        class AnyChecker : public IComponentChecker {
        public:
            virtual bool belong(const Size UNUSED(i), const Size UNUSED(j)) override {
                // any two particles within the search radius belong to the same component
                return true;
            }
        };
        checker = makeAuto<AnyChecker>();
        break;
    default:
        NOT_IMPLEMENTED;
    }

    ASSERT(checker);

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);

    Size componentCnt = findComponentsImpl(*checker, r, radius, indices);

    if (connectivity == ComponentConnectivity::ESCAPE_VELOCITY) {
        // now we have to merge components with relative velocity lower than the (mutual) escape velocity

        // first, compute the total mass and the average velocity of each component
        Array<Float> masses(componentCnt);
        Array<Vector> positions(componentCnt);
        Array<Vector> velocities(componentCnt);
        Array<Float> volumes(componentCnt);

        masses.fill(0._f);
        positions.fill(Vector(0._f));
        velocities.fill(Vector(0._f));
        volumes.fill(0._f);

        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);

        for (Size i = 0; i < r.size(); ++i) {
            masses[indices[i]] += m[i];
            positions[indices[i]] += m[i] * r[i];
            velocities[indices[i]] += m[i] * v[i];
            volumes[indices[i]] += pow<3>(r[i][H]);
        }
        for (Size k = 0; k < componentCnt; ++k) {
            ASSERT(masses[k] > 0._f);
            positions[k] /= masses[k];
            positions[k][H] = cbrt(3._f * volumes[k] / (4._f * PI));
            velocities[k] /= masses[k];
        }

        // Helper checker connecting components with relative velocity lower than v_esc
        struct EscapeVelocityComponentChecker : public IComponentChecker {
            ArrayView<const Vector> r;
            ArrayView<const Vector> v;
            ArrayView<const Float> m;
            Float radius;

            virtual bool belong(const Size i, const Size j) override {
                const Float dv = getLength(v[i] - v[j]);
                const Float dr = getLength(r[i] - r[j]);
                const Float m_tot = m[i] + m[j];
                const Float v_esc = sqrt(2._f * Constants::gravity * m_tot / dr);
                return dv < v_esc;
            }
        };
        EscapeVelocityComponentChecker velocityChecker;
        velocityChecker.r = positions;
        velocityChecker.v = velocities;
        velocityChecker.m = masses;
        velocityChecker.radius = radius;

        // run the component finder again, this time for the components found in the first step
        Array<Size> velocityIndices;
        componentCnt = findComponentsImpl(velocityChecker, positions, 50._f, velocityIndices);

        // We should keep merging the components, as now we could have created a new component that was
        // previously undetected (three bodies bound to their center of gravity, where each two bodies move
        // faster than the escape velocity of those two bodies). That is not very probable, though, so we end
        // the process here.

        // Last thing - we have to reindex the components found in the first step, using the indices from the
        // second step.
        ASSERT(r.size() == indices.size());
        for (Size i = 0; i < r.size(); ++i) {
            indices[i] = velocityIndices[indices[i]];
        }
    }

    return componentCnt;
}

Array<Size> Post::findLargestComponent(const Storage& storage, const Float particleRadius) {
    Array<Size> componentIdxs;
    const Size componentCnt =
        Post::findComponents(storage, particleRadius, ComponentConnectivity::OVERLAP, componentIdxs);

    // find the masses of each component
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    Array<Float> masses(componentCnt);
    masses.fill(0);
    for (Size i = 0; i < m.size(); ++i) {
        masses[componentIdxs[i]] += m[i];
    }

    // find the largest remnant
    const Size largestComponentIdx =
        Size(std::distance(masses.begin(), std::max_element(masses.begin(), masses.end())));

    // get the indices
    Array<Size> idxs;
    for (Size i = 0; i < m.size(); ++i) {
        if (componentIdxs[i] == largestComponentIdx) {
            idxs.push(i);
        }
    }
    return idxs;
}


/*static Storage clone(const Storage& storage) {
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
}*/

/*Storage Post::findFutureBodies2(const Storage& storage, ILogger& logger) {
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
}*/

Array<Post::MoonEnum> Post::findMoons(const Storage& storage, const Float radius, const Float limit) {
    // first, find the larget one
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    const auto largestIter = std::max_element(m.begin(), m.end());
    const Float largestM = *largestIter;
    const Size largestIdx = std::distance(m.begin(), largestIter);

    Array<MoonEnum> statuses(m.size());
#ifdef SPH_DEBUG
    statuses.fill(MoonEnum(-1));
#endif
    statuses[largestIdx] = MoonEnum::LARGEST_FRAGMENT;

    // find the ellipse for all bodies
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < m.size(); ++i) {
        if (i == largestIdx) {
            continue;
        }

        // check for observability
        if (r[i][H] < limit * r[largestIdx][H]) {
            statuses[i] = MoonEnum::UNOBSERVABLE;
            continue;
        }

        // compute the orbital elements
        Optional<KeplerianElements> elements = findKeplerEllipse(
            m[i] + largestM, m[i] * largestM / (m[i] + largestM), r[i] - r[largestIdx], v[i] - v[largestIdx]);

        if (!elements) {
            // not bound, mark as ejected body
            statuses[i] = MoonEnum::RUNAWAY;
        } else {
            // if the pericenter is closer than the sum of radii, mark as impactor
            if (elements->pericenterDist() < radius * (r[i][H] + r[largestIdx][H])) {
                statuses[i] = MoonEnum::IMPACTOR;
            } else {
                // bound and not on collisional trajectory
                statuses[i] = MoonEnum::MOON;
            }
        }
    }

    return statuses;
}

Array<Post::Tumbler> Post::findTumblers(const Storage& storage, const Float limit) {
    Array<Tumbler> tumblers;
    ArrayView<const Vector> omega = storage.getValue<Vector>(QuantityId::ANGULAR_FREQUENCY);
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

Vector Post::getAngularFrequency(ArrayView<const Float> m,
    ArrayView<const Vector> r,
    ArrayView<const Vector> v,
    const Vector& r0,
    const Vector& v0) {
    SymmetricTensor I = getInertiaTensor(m, r, r0);
    Vector L(0._f);
    for (Size i = 0; i < r.size(); ++i) {
        L += m[i] * cross(r[i] - r0, v[i] - v0);
    }
    // L = I * omega => omega = I^-1 * L)
    const SymmetricTensor I_inv = I.inverse();
    ASSERT(isReal(I_inv));
    return I_inv * L;
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

bool Post::HistPoint::operator==(const HistPoint& other) const {
    return value == other.value && count == other.count;
}

/// \brief Filters the input values using cut-offs specified in params.
template <typename TValueFunctor>
static Array<Float> processParticleCutoffs(const Storage& storage,
    const Post::HistogramParams& params,
    const TValueFunctor& functor) {
    ArrayView<const Float> m;
    ArrayView<const Vector> v;

    // only require mass/velocity if the correpsonding cutoff is specified
    if (params.massCutoff > 0._f) {
        m = storage.getValue<Float>(QuantityId::MASS);
    }
    if (params.velocityCutoff < INFTY) {
        v = storage.getDt<Vector>(QuantityId::POSITION);
    }
    Array<Float> filtered;
    for (Size i = 0; i < storage.getParticleCnt(); ++i) {
        if (m && m[i] < params.massCutoff) {
            continue;
        }
        if (v && getLength(v[i]) > params.velocityCutoff) {
            continue;
        }
        if (params.validator(i)) {
            filtered.push(functor(i));
        }
    }
    return filtered;
}

/// \brief Returns the particle values corresponding to given histogram quantity.
static Array<Float> getParticleValues(const Storage& storage,
    const Post::HistogramParams& params,
    const Post::HistogramId id) {

    switch (id) {
    case Post::HistogramId::RADII: {
        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        return processParticleCutoffs(storage, params, [r](Size i) { return r[i][H]; });
    }
    case Post::HistogramId::EQUIVALENT_MASS_RADII: {
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        return processParticleCutoffs(storage, params, [m, &params](Size i) {
            return root<3>(3.f * m[i] / (params.referenceDensity * 4.f * PI));
        });
    }
    case Post::HistogramId::VELOCITIES: {
        ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
        return processParticleCutoffs(storage, params, [v](Size i) { return getLength(v[i]); });
    }
    case Post::HistogramId::ROTATIONAL_PERIOD: {
        if (!storage.has(QuantityId::ANGULAR_FREQUENCY)) {
            return {};
        }
        ArrayView<const Vector> omega = storage.getValue<Vector>(QuantityId::ANGULAR_FREQUENCY);
        return processParticleCutoffs(storage, params, [omega](Size i) {
            const Float w = getLength(omega[i]);
            if (w == 0._f) {
                // placeholder for zero frequency to avoid division by zero
                return 0._f;
            } else {
                return 2._f * PI / (3600._f * w);
            }
        });
    }
    case Post::HistogramId::ROTATIONAL_FREQUENCY: {
        if (!storage.has(QuantityId::ANGULAR_FREQUENCY)) {
            return {};
        }
        ArrayView<const Vector> omega = storage.getValue<Vector>(QuantityId::ANGULAR_FREQUENCY);
        return processParticleCutoffs(storage, params, [omega](Size i) {
            const Float w = getLength(omega[i]);
            return 3600._f * 24._f * w / (2._f * PI);
        });
    }
    case Post::HistogramId::ROTATIONAL_AXIS: {
        if (!storage.has(QuantityId::ANGULAR_FREQUENCY)) {
            return {};
        }
        ArrayView<const Vector> omega = storage.getValue<Vector>(QuantityId::ANGULAR_FREQUENCY);
        return processParticleCutoffs(storage, params, [omega](Size i) {
            const Float w = getLength(omega[i]);
            if (w == 0._f) {
                return 0._f; /// \todo what here??
            } else {
                return acos(omega[i][Z] / w);
            }
        });
    }
    default:
        const QuantityId quantityId = QuantityId(id);
        ASSERT((int)quantityId >= 0);
        /// \todo allow also other types
        Array<Float> values = storage.getValue<Float>(quantityId).clone();
        return processParticleCutoffs(storage, params, [&values](Size i) { return values[i]; });
    }
}

/// \brief Returns indices of components to remove from the histogram.
static Array<Size> processComponentCutoffs(const Storage& storage,
    ArrayView<const Size> components,
    const Size numComponents,
    const Post::HistogramParams& params) {
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
    Array<Vector> velocities(numComponents);
    Array<Float> masses(numComponents);
    velocities.fill(Vector(0._f));
    masses.fill(0._f);

    for (Size i = 0; i < m.size(); ++i) {
        velocities[components[i]] += m[i] * v[i];
        masses[components[i]] += m[i];
    }

    Array<Size> toRemove;
    for (Size idx = 0; idx < numComponents; ++idx) {
        ASSERT(masses[idx] > 0._f);
        velocities[idx] /= masses[idx];

        if (masses[idx] < params.massCutoff || getLength(velocities[idx]) > params.velocityCutoff) {
            toRemove.push(idx);
        }
    }
    return toRemove;
}

/// \todo move directly to Storage?
struct MissingQuantityException : public std::exception {
private:
    std::string message;

public:
    explicit MissingQuantityException(const QuantityId id) {
        message = "Attempting to access missing quantity " + getMetadata(id).quantityName;
    }


    virtual const char* what() const noexcept override {
        return message.c_str();
    }
};

/// \brief Returns the component values corresponding to given histogram quantity.
static Array<Float> getComponentValues(const Storage& storage,
    const Post::HistogramParams& params,
    const Post::HistogramId id) {

    Array<Size> components;
    const Size numComponents =
        findComponents(storage, params.components.radius, params.components.connectivity, components);

    Array<Size> toRemove = processComponentCutoffs(storage, components, numComponents, params);

    switch (id) {
    case Post::HistogramId::EQUIVALENT_MASS_RADII:
    case Post::HistogramId::RADII: {
        // compute volume of the body
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        ArrayView<const Float> rho;
        if (id == Post::HistogramId::RADII) {
            if (storage.has(QuantityId::DENSITY)) {
                rho = storage.getValue<Float>(QuantityId::DENSITY);
            } else {
                throw MissingQuantityException(QuantityId::DENSITY);
            }
        }

        Array<Float> values(numComponents);
        values.fill(0._f);
        for (Size i = 0; i < rho.size(); ++i) {
            Float density;
            if (id == Post::HistogramId::EQUIVALENT_MASS_RADII) {
                density = params.referenceDensity;
            } else {
                density = rho[i];
            }
            ASSERT(m[i] > 0._f && density > 0._f);
            values[components[i]] += m[i] / density;
        }

        // remove the components we cut off (in reverse to avoid invalidating indices)
        for (Size i : reverse(toRemove)) {
            values.remove(i);
        }

        // compute equivalent radii from volumes
        Array<Float> radii(values.size());
        for (Size i = 0; i < values.size(); ++i) {
            radii[i] = root<3>(3._f * values[i] / (4._f * PI));
            ASSERT(isReal(radii[i]) && radii[i] > 0._f, values[i]);
        }
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

        // remove the components we cut off (in reverse to avoid invalidating indices)
        for (Size i : reverse(toRemove)) {
            sumV.remove(i);
            weights.remove(i);
        }

        Array<Float> velocities(sumV.size());
        for (Size i = 0; i < sumV.size(); ++i) {
            ASSERT(weights[i] != 0._f);
            velocities[i] = getLength(sumV[i] / weights[i]);
        }
        return velocities;
    }
    default:
        NOT_IMPLEMENTED;
    }
}

/// \brief Returns the values of particles or components of particles
static Array<Float> getValues(const Storage& storage,
    const Post::HistogramId id,
    const Post::HistogramSource source,
    const Post::HistogramParams& params) {
    Array<Float> values;
    if (source == Post::HistogramSource::PARTICLES) {
        values = getParticleValues(storage, params, id);
        ASSERT(values.size() <= storage.getParticleCnt()); // can be < due to cutoffs
    } else {
        values = getComponentValues(storage, params, id);
        ASSERT(values.size() <= storage.getParticleCnt());
    }
    return values;
}

Array<Post::HistPoint> Post::getCumulativeHistogram(const Storage& storage,
    const HistogramId id,
    const HistogramSource source,
    const Post::HistogramParams& params) {

    Array<Float> values = getValues(storage, id, source, params);
    if (values.empty()) {
        return {}; // no values, trivially empty histogram
    }
    std::sort(values.begin(), values.end());

    Interval range = params.range;
    if (range.empty()) {
        for (Size i = 0; i < values.size(); ++i) {
            range.extend(values[i]);
        }
    }
    ASSERT(!range.empty());

    Array<HistPoint> histogram;
    Size count = 1;
    Float lastR = INFTY;

    // iterate in reverse order - from largest radii to smallest ones
    for (int i = values.size() - 1; i >= 0; --i) {
        if (values[i] < lastR) {
            if (range.contains(values[i])) {
                histogram.push(HistPoint{ values[i], count });
            }
            lastR = values[i];
        }
        count++;
    }
    ASSERT(histogram.size() > 0);

    return histogram;
}

Array<Post::HistPoint> Post::getDifferentialHistogram(const Storage& storage,
    const HistogramId id,
    const HistogramSource source,
    const HistogramParams& params) {

    Array<Float> values = getValues(storage, id, source, params);
    return getDifferentialHistogram(values, params);
}

Array<Post::HistPoint> Post::getDifferentialHistogram(ArrayView<const Float> values,
    const HistogramParams& params) {

    Interval range = params.range;
    if (range.empty()) {
        for (Size i = 0; i < values.size(); ++i) {
            range.extend(values[i]);
        }
        // extend slightly, so that the min/max value is strictly inside the interval
        range.extend(range.lower() - EPS * range.size());
        range.extend(range.upper() + EPS * range.size());
    }
    ASSERT(!range.empty());
    ASSERT(isReal(range.lower()) && isReal(range.upper()));

    Size binCnt = params.binCnt;
    if (binCnt == 0) {
        // estimate initial bin count as sqrt of component count
        binCnt = Size(sqrt(values.size()));
    }

    Array<Size> sfd(binCnt);
    sfd.fill(0);
    // check for case where only one body/particle exists
    const bool singular = range.size() == 0;
    for (Size i = 0; i < values.size(); ++i) {
        // get bin index
        Size binIdx;
        if (singular) {
            binIdx = 0; // just add everything into the first bin to get some reasonable output
        } else {
            const Float floatIdx = binCnt * (values[i] - range.lower()) / range.size();
            if (floatIdx >= 0._f && floatIdx < binCnt) {
                binIdx = Size(floatIdx);
            } else {
                // out of range, skip
                // this should not happen if the range was determined
                ASSERT(!params.range.empty(), floatIdx, binCnt);
                continue;
            }
        }
        sfd[binIdx]++;
    }
    // convert to HistPoints
    Array<HistPoint> histogram(binCnt);
    for (Size i = 0; i < binCnt; ++i) {
        const Float centerIdx = i + int(params.centerBins) * 0.5_f;
        histogram[i] = { range.lower() + (centerIdx * range.size()) / binCnt, sfd[i] };
        ASSERT(isReal(histogram[i].value), sfd[i], range);
    }
    return histogram;
}

NAMESPACE_SPH_END
