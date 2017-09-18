#include "post/Analysis.h"
#include "io/Column.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "objects/finders/BruteForceFinder.h"
#include "objects/finders/Voxel.h"
#include "objects/geometry/Box.h"
#include "objects/utility/Iterators.h"
#include "quantities/Storage.h"
#include "sph/kernel/KernelFactory.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

Size Post::findComponents(const Storage& storage,
    const Float radius,
    const ComponentConnectivity connectivity,
    Array<Size>& indices) {
    ASSERT(radius > 0._f);
    // get values from storage
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);

    AutoPtr<INeighbourFinder> finder;
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
        finder = makeAuto<VoxelFinder>();
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
        finder = makeAuto<VoxelFinder>();
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
                r = storage.getValue<Vector>(QuantityId::POSITIONS);
                v = storage.getDt<Vector>(QuantityId::POSITIONS);
                m = storage.getValue<Float>(QuantityId::MASSES);
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
        finder = makeAuto<VoxelFinder>();
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
                finder->findNeighbours(index, r[index][H] * actRadius, neighs);
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
    const Array<Vector>& r = storage.getValue<Vector>(QuantityId::POSITIONS);
    cloned.insert<Vector>(QuantityId::POSITIONS, OrderEnum::FIRST, r.clone());

    const Array<Vector>& v = storage.getDt<Vector>(QuantityId::POSITIONS);
    cloned.getDt<Vector>(QuantityId::POSITIONS) = v.clone();

    if (storage.has(QuantityId::MASSES)) {
        const Array<Float>& m = storage.getValue<Float>(QuantityId::MASSES);
        cloned.insert<Float>(QuantityId::MASSES, OrderEnum::ZERO, m.clone());
    } else {
        ArrayView<const Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
        Float rhoAvg = 0._f;
        for (Size i = 0; i < r.size(); ++i) {
            rhoAvg += rho[i];
        }
        rhoAvg /= r.size();

        /// \todo ASSUMING 10km body!
        const Float m = sphereVolume(5.e3_f) * rhoAvg / r.size();
        cloned.insert<Float>(QuantityId::MASSES, OrderEnum::ZERO, m);
    }

    return cloned;
}

Storage Post::findFutureBodies2(const Storage& storage, ILogger& logger) {
    Array<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS).clone();
    Array<Vector> v = storage.getDt<Vector>(QuantityId::POSITIONS).clone();
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


        ArrayView<const Vector> r = cloned.getValue<Vector>(QuantityId::POSITIONS);
        ArrayView<const Vector> v = cloned.getDt<Vector>(QuantityId::POSITIONS);
        ArrayView<const Float> m = cloned.getValue<Float>(QuantityId::MASSES);

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

        cloned.getValue<Vector>(QuantityId::POSITIONS) = std::move(r_new);
        cloned.getDt<Vector>(QuantityId::POSITIONS) = std::move(v_new);
        cloned.getValue<Float>(QuantityId::MASSES) = std::move(m_new);

        iter++;
    } while (numComponents != prevNumComponents);

    return cloned;
}

static Array<Float> getBodiesRadii(const Storage& storage,
    const Post::HistogramParams& params,
    const Post::HistogramId id) {
    switch (params.source) {
    case Post::HistogramParams::Source::PARTICLES: {
        Array<Float> values(storage.getParticleCnt());
        switch (id) {
        case Post::HistogramId::RADII: {
            ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
            for (Size i = 0; i < r.size(); ++i) {
                values[i] = r[i][H];
            }
            break;
        }
        case Post::HistogramId::VELOCITIES: {
            ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITIONS);
            for (Size i = 0; i < v.size(); ++i) {
                values[i] = getLength(v[i]);
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
            tie(rho, m) = storage.getValues<Float>(QuantityId::DENSITY, QuantityId::MASSES);
            for (Size i = 0; i < rho.size(); ++i) {
                values[components[i]] += m[i] / rho[i];
            }

            // compute equivalent radii from volumes
            Array<Float> radii(numComponents);
            for (Size i = 0; i < numComponents; ++i) {
                radii[i] = root<3>(values[i]);
            }
            std::sort(radii.begin(), radii.end());
            return radii;
        }
        case Post::HistogramId::VELOCITIES: {
            // compute velocity as weighted average
            ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASSES);
            ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITIONS);
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
            range.extend(r);
        }
    }

    Array<SfdPoint> histogram;
    Size count = 1;
    Float lastR = INFTY;
    // iterate in reverse order - from largest radii to smallest ones
    for (Float r : reverse(radii)) {
        if (r < lastR) {
            if (range.contains(r)) {
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
    if (range == Interval::unbounded()) {
        for (Float r : radii) {
            range.extend(r);
        }
    }

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
    }
    return histogram;
}

template <typename TValue>
static void sort(Array<TValue>& ar, const Order& order) {
    Array<TValue> ar0 = ar.clone();
    for (Size i = 0; i < ar.size(); ++i) {
        ar[i] = ar0[order[i]];
    }
}

Expected<Storage> Post::parsePkdgravOutput(const Path& path) {
    /* the extension can also be '.50000.bt' or '.last.bt'
     *  if (path.extension() != Path("bt")) {
        return makeUnexpected<Storage>(
            "Invalid extension of pkdgrav file " + path.native() + ", expected '.bt'");
    }*/

    TextOutput output;

    // 1) Particle index -- we don't really need that, just add dummy columnm
    class DummyColumn : public ITextColumn {
    private:
        ValueEnum type;

    public:
        DummyColumn(const ValueEnum type)
            : type(type) {}

        virtual Value evaluate(const Storage&, const Statistics&, const Size) const override {
            NOT_IMPLEMENTED;
        }

        virtual void accumulate(Storage&, const Value, const Size) const override {}

        virtual std::string getName() const override {
            return "dummy";
        }

        virtual ValueEnum getType() const override {
            return type;
        }
    };
    output.add(makeAuto<DummyColumn>(ValueEnum::INDEX));

    // 2) Original index -- not really needed, skip
    output.add(makeAuto<DummyColumn>(ValueEnum::INDEX));

    // 3) Particle mass
    output.add(makeAuto<ValueColumn<Float>>(QuantityId::MASSES));

    // 4) radius ?  -- skip
    output.add(makeAuto<ValueColumn<Float>>(QuantityId::DENSITY));

    // 5) Positions (3 components)
    output.add(makeAuto<ValueColumn<Vector>>(QuantityId::POSITIONS));

    // 6) Velocities (3 components)
    output.add(makeAuto<DerivativeColumn<Vector>>(QuantityId::POSITIONS));

    // 7) Angular velocities ? (3 components)
    output.add(makeAuto<ValueColumn<Vector>>(QuantityId::ANGULAR_VELOCITY));

    // 8) Color index -- skip
    output.add(makeAuto<DummyColumn>(ValueEnum::INDEX));

    Storage storage;
    Outcome outcome = output.load(path, storage);
    if (!outcome) {
        return makeUnexpected<Storage>(outcome.error());
    }

    // Convert units -- assuming default conversion values
    PkdgravParams::Conversion conversion;
    Array<Vector>& r = storage.getValue<Vector>(QuantityId::POSITIONS);
    Array<Vector>& v = storage.getDt<Vector>(QuantityId::POSITIONS);
    Array<Float>& m = storage.getValue<Float>(QuantityId::MASSES);
    Array<Float>& rho = storage.getValue<Float>(QuantityId::DENSITY);
    for (Size i = 0; i < r.size(); ++i) {
        r[i] *= conversion.distance;
        v[i] *= conversion.velocity;
        m[i] *= conversion.mass;

        // compute radius, using the density formula
        /// \todo here we actually store radius in rho ...
        rho[i] *= conversion.distance;
        r[i][H] = root<3>(3.f * m[i] / (2700.f * 4.f * PI));

        // replace the radius with actual density
        /// \todo too high, fix
        rho[i] = m[i] / pow<3>(rho[i]);
    }

    // sort
    Order order(r.size());
    order.shuffle([&m](const Size i1, const Size i2) { return m[i1] > m[i2]; });
    sort(r, order);
    sort(v, order);
    sort(m, order);
    sort(rho, order);
    return std::move(storage);
}

NAMESPACE_SPH_END
