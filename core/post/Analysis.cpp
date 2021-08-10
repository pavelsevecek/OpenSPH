#include "post/Analysis.h"
#include "io/Column.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "objects/finders/BruteForceFinder.h"
#include "objects/finders/UniformGrid.h"
#include "objects/geometry/Box.h"
#include "objects/utility/IteratorAdapters.h"
#include "post/MarchingCubes.h"
#include "post/Point.h"
#include "post/TwoBody.h"
#include "quantities/Storage.h"
#include "quantities/Utility.h"
#include "sph/initial/MeshDomain.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"
#include "thread/Scheduler.h"
#include <numeric>
#include <set>

NAMESPACE_SPH_BEGIN

Array<Size> Post::findNeighborCounts(const Storage& storage, const Float particleRadius) {
    Array<NeighborRecord> neighs;
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
struct ComponentChecker : public Polymorphic {
    virtual bool belong(const Size UNUSED(i), const Size UNUSED(j)) {
        // by default, any two particles within the search radius belong to the same component
        return true;
    }
};

static Size findComponentsImpl(ComponentChecker& checker,
    ArrayView<const Vector> r,
    const Float radius,
    Array<Size>& indices) {
    // initialize stuff
    indices.resize(r.size());
    const Size unassigned = Size(-1);
    indices.fill(unassigned);
    Size componentIdx = 0;

    Array<Size> stack;
    Array<NeighborRecord> neighs;

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
                        // do not count as neighbors
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
    const Flags<ComponentFlag> flags,
    Array<Size>& indices) {
    SPH_ASSERT(radius > 0._f);

    AutoPtr<ComponentChecker> checker = makeAuto<ComponentChecker>();

    if (flags.has(ComponentFlag::SEPARATE_BY_FLAG)) {
        class FlagComponentChecker : public ComponentChecker {
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
    }

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    Size componentCnt = findComponentsImpl(*checker, r, radius, indices);

    if (flags.has(ComponentFlag::ESCAPE_VELOCITY)) {
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
        ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);

        for (Size i = 0; i < r.size(); ++i) {
            masses[indices[i]] += m[i];
            positions[indices[i]] += m[i] * r[i];
            velocities[indices[i]] += m[i] * v[i];
            volumes[indices[i]] += pow<3>(r[i][H]);
        }
        for (Size k = 0; k < componentCnt; ++k) {
            SPH_ASSERT(masses[k] > 0._f);
            positions[k] /= masses[k];
            positions[k][H] = cbrt(3._f * volumes[k] / (4._f * PI));
            velocities[k] /= masses[k];
        }

        // Helper checker connecting components with relative velocity lower than v_esc
        struct EscapeVelocityComponentChecker : public ComponentChecker {
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
        SPH_ASSERT(r.size() == indices.size());
        for (Size i = 0; i < r.size(); ++i) {
            indices[i] = velocityIndices[indices[i]];
        }
    }

#ifdef SPH_DEBUG
    std::set<Size> uniqueIndices;
    for (Size i : indices) {
        uniqueIndices.insert(i);
    }
    SPH_ASSERT(uniqueIndices.size() == componentCnt);
    Size counter = 0;
    for (Size i : uniqueIndices) {
        SPH_ASSERT(i == counter);
        counter++;
    }

#endif

    if (flags.has(ComponentFlag::SORT_BY_MASS)) {
        Array<Float> componentMass(componentCnt);
        componentMass.fill(0._f);
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        for (Size i = 0; i < indices.size(); ++i) {
            componentMass[indices[i]] += m[i];
        }
        Order mapping(componentCnt);
        mapping.shuffle([&componentMass](Size i, Size j) { return componentMass[i] > componentMass[j]; });
        mapping = mapping.getInverted();

        Array<Size> realIndices(indices.size());
        for (Size i = 0; i < indices.size(); ++i) {
            realIndices[i] = mapping[indices[i]];
        }

        indices = copyable(realIndices);
    }

    return componentCnt;
}

Array<Size> Post::findLargestComponent(const Storage& storage,
    const Float particleRadius,
    const Flags<ComponentFlag> flags) {
    Array<Size> componentIdxs;
    Post::findComponents(storage, particleRadius, flags | ComponentFlag::SORT_BY_MASS, componentIdxs);

    // get the indices of the largest component (with index 0)
    Array<Size> idxs;
    for (Size i = 0; i < componentIdxs.size(); ++i) {
        if (componentIdxs[i] == 0) {
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
            SPH_ASSERT(isReal(W_tot));
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
                SPH_ASSERT(W_tot > 0._f);
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
            SPH_ASSERT(m_new[i] != 0._f);
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
        Optional<Kepler::Elements> elements = Kepler::computeOrbitalElements(
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

Size Post::findMoonCount(ArrayView<const Float> m,
    ArrayView<const Vector> r,
    ArrayView<const Vector> v,
    const Size i,
    const Float radius,
    const Float limit) {
    SPH_ASSERT(std::is_sorted(m.begin(), m.end(), std::greater<Float>{}));
    SPH_ASSERT(r.size() == m.size());

    Size count = 0;
    for (Size j = i + 1; j < r.size(); ++j) {
        if (m[j] < limit * m[i]) {
            break;
        }

        Optional<Kepler::Elements> elements = Kepler::computeOrbitalElements(
            m[i] + m[j], m[i] * m[j] / (m[i] + m[j]), r[i] - r[j], v[i] - v[j]);

        if (elements && elements->pericenterDist() > radius * (r[i][H] + r[j][H])) {
            count++;
        }
    }

    return count;
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
        SPH_ASSERT(cosBeta >= -1._f && cosBeta <= 1._f);
        const Float beta = acos(cosBeta);
        if (beta > limit) {
            tumblers.push(Tumbler{ i, beta });
        }
    }
    return tumblers;
}

Vector Post::getCenterOfMass(ArrayView<const Float> m,
    ArrayView<const Vector> r,
    ArrayView<const Size> idxs) {
    Vector r_com(0._f);
    Float m_tot = 0._f;
    auto functor = [&m_tot, &r_com, m, r](Size i) {
        r_com += m[i] * r[i];
        m_tot += m[i];
    };
    if (idxs) {
        for (Size i : idxs) {
            functor(i);
        }
    } else {
        for (Size i = 0; i < r.size(); ++i) {
            functor(i);
        }
    }
    r_com /= m_tot;
    r_com[H] = 0._f;
    return r_com;
}

SymmetricTensor Post::getInertiaTensor(ArrayView<const Float> m,
    ArrayView<const Vector> r,
    const Vector& r0,
    ArrayView<const Size> idxs) {
    SymmetricTensor I = SymmetricTensor::null();

    auto functor = [&I, r, m, &r0](Size i) {
        const Vector dr = r[i] - r0;
        I += m[i] * (SymmetricTensor::identity() * getSqrLength(dr) - symmetricOuter(dr, dr));
    };
    if (idxs) {
        for (Size i : idxs) {
            functor(i);
        }
    } else {
        for (Size i = 0; i < r.size(); ++i) {
            functor(i);
        }
    }
    return I;
}

SymmetricTensor Post::getInertiaTensor(ArrayView<const Float> m,
    ArrayView<const Vector> r,
    ArrayView<const Size> idxs) {

    const Vector r_com = getCenterOfMass(m, r, idxs);
    return getInertiaTensor(m, r, r_com, idxs);
}

Vector Post::getAngularFrequency(ArrayView<const Float> m,
    ArrayView<const Vector> r,
    ArrayView<const Vector> v,
    const Vector& r0,
    const Vector& v0,
    ArrayView<const Size> idxs) {
    SymmetricTensor I = getInertiaTensor(m, r, r0, idxs);
    Vector L(0._f);
    auto functor = [&L, m, r, v, &r0, &v0](const Size i) { //
        L += m[i] * cross(r[i] - r0, v[i] - v0);
    };

    if (idxs) {
        for (Size i : idxs) {
            functor(i);
        }
    } else {
        for (Size i = 0; i < r.size(); ++i) {
            functor(i);
        }
    }
    // L = I * omega => omega = I^-1 * L)
    const SymmetricTensor I_inv = I.inverse();
    SPH_ASSERT(isReal(I_inv));
    return I_inv * L;
}

Vector Post::getAngularFrequency(ArrayView<const Float> m,
    ArrayView<const Vector> r,
    ArrayView<const Vector> v,
    ArrayView<const Size> idxs) {
    const Vector r_com = getCenterOfMass(m, r, idxs);
    const Vector v_com = getCenterOfMass(m, v, idxs);
    return getAngularFrequency(m, r, v, r_com, v_com, idxs);
}

Float Post::getSphericity(IScheduler& scheduler, const Storage& storage, const Float resolution) {
    const Box boundingBox = getBoundingBox(storage);
    McConfig config;
    config.gridResolution = resolution * maxElement(boundingBox.size());
    config.surfaceLevel = 0.15_f;
    Array<Triangle> mesh = getSurfaceMesh(scheduler, storage, config);
    Float area = 0._f;
    for (const Triangle& triangle : mesh) {
        area += triangle.area();
    }
    SPH_ASSERT(area > 0._f);

    MeshParams params;
    params.precomputeInside = false;
    MeshDomain domain(scheduler, std::move(mesh), params);
    const Float volume = domain.getVolume();
    SPH_ASSERT(volume > 0._f);

    // https://en.wikipedia.org/wiki/Sphericity
    return pow(PI * sqr(6._f * volume), 1._f / 3._f) / area;
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
            return root<3>(3._f * m[i] / (params.referenceDensity * 4._f * PI));
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
        SPH_ASSERT((int)quantityId >= 0);
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
        SPH_ASSERT(masses[idx] > 0._f);
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
        findComponents(storage, params.components.radius, params.components.flags, components);

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
        for (Size i = 0; i < m.size(); ++i) {
            Float density;
            if (id == Post::HistogramId::EQUIVALENT_MASS_RADII) {
                density = params.referenceDensity;
            } else {
                density = rho[i];
            }
            SPH_ASSERT(m[i] > 0._f && density > 0._f);
            values[components[i]] += m[i] / density;
        }

        // remove the components we cut off (in reverse to avoid invalidating indices)
        values.remove(toRemove);

        // compute equivalent radii from volumes
        Array<Float> radii(values.size());
        for (Size i = 0; i < values.size(); ++i) {
            radii[i] = root<3>(3._f * values[i] / (4._f * PI));
            SPH_ASSERT(isReal(radii[i]) && radii[i] > 0._f, values[i]);
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
        sumV.remove(toRemove);
        weights.remove(toRemove);

        Array<Float> velocities(sumV.size());
        for (Size i = 0; i < sumV.size(); ++i) {
            SPH_ASSERT(weights[i] != 0._f);
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
    const Post::ExtHistogramId id,
    const Post::HistogramSource source,
    const Post::HistogramParams& params) {
    Array<Float> values;
    if (source == Post::HistogramSource::PARTICLES) {
        values = getParticleValues(storage, params, id);
        SPH_ASSERT(values.size() <= storage.getParticleCnt()); // can be < due to cutoffs
    } else {
        values = getComponentValues(storage, params, id);
        SPH_ASSERT(values.size() <= storage.getParticleCnt());
    }
    return values;
}

Array<Post::HistPoint> Post::getCumulativeHistogram(const Storage& storage,
    const ExtHistogramId id,
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
    SPH_ASSERT(!range.empty());

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
    SPH_ASSERT(histogram.size() > 0);

    return histogram;
}

Array<Post::HistPoint> Post::getDifferentialHistogram(const Storage& storage,
    const ExtHistogramId id,
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
    SPH_ASSERT(!range.empty());
    SPH_ASSERT(isReal(range.lower()) && isReal(range.upper()));

    Size binCnt = params.binCnt;
    if (binCnt == 0) {
        // estimate initial bin count as sqrt of component count
        binCnt = Size(0.5 * sqrt(Float(values.size())));
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
                SPH_ASSERT(!params.range.empty(), floatIdx, binCnt);
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
        SPH_ASSERT(isReal(histogram[i].value), sfd[i], range);
    }
    return histogram;
}

Post::LinearFunction Post::getLinearFit(ArrayView<const PlotPoint> points) {
    SPH_ASSERT(points.size() >= 2);
    Float x = 0._f, x2 = 0._f;
    Float y = 0._f, y2 = 0._f;
    Float xy = 0._f;
    for (PlotPoint p : points) {
        x += p.x;
        x2 += sqr(p.x);
        y += p.y;
        y2 += sqr(p.y);
        xy += p.x * p.y;
    }

    const Size n = points.size();
    const Float denom = n * x2 - sqr(x);
    SPH_ASSERT(denom > 0._f);
    const Float b = (y * x2 - x * xy) / denom;
    const Float a = (n * xy - x * y) / denom;
    return LinearFunction(a, b);
}

Post::QuadraticFunction Post::getQuadraticFit(ArrayView<const PlotPoint> points) {
    SPH_ASSERT(points.size() >= 3);
    Array<Vector> xs(points.size());
    Array<Float> ys(points.size());
    for (Size k = 0; k < xs.size(); ++k) {
        xs[k] = Vector(1._f, points[k].x, sqr(points[k].x));
        ys[k] = points[k].y;
    }
    AffineMatrix xTx = AffineMatrix::null();
    Vector xTy = Vector(0._f);

    for (Size k = 0; k < xs.size(); ++k) {
        for (Size i = 0; i < 3; ++i) {
            for (Size j = 0; j < 3; ++j) {
                xTx(i, j) += xs[k][j] * xs[k][i];
            }
            xTy[i] += xs[k][i] * ys[k];
        }
    }
    SPH_ASSERT(xTx.determinant() != 0._f);

    const Vector result = xTx.inverse() * xTy;
    return QuadraticFunction(result[2], result[1], result[0]);
}

NAMESPACE_SPH_END
