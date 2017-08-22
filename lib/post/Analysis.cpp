#include "post/Analysis.h"
#include "io/Column.h"
#include "io/Output.h"
#include "objects/geometry/Box.h"
#include "objects/wrappers/Iterators.h"
#include "quantities/Storage.h"
#include "sph/kernel/KernelFactory.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

Size Post::findComponents(const Storage& storage,
    const RunSettings& settings,
    const ComponentConnectivity connectivity,
    Array<Size>& indices) {
    // get values from storage
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);

    const bool separateFlag = (connectivity == ComponentConnectivity::SEPARATE_BY_FLAG);
    ArrayView<const Size> flag;
    if (separateFlag) {
        flag = storage.getValue<Size>(QuantityId::FLAG);
    }

    // initialize stuff
    indices.resize(r.size());
    const Size unassigned = std::numeric_limits<Size>::max();
    indices.fill(unassigned);
    Size componentIdx = 0;
    AutoPtr<Abstract::Finder> finder = Factory::getFinder(settings);
    finder->build(r);
    const Float radius = Factory::getKernel<3>(settings).radius();

    Array<Size> stack;
    Array<NeighbourRecord> neighs;

    for (Size i = 0; i < r.size(); ++i) {
        if (indices[i] == unassigned) {
            indices[i] = componentIdx;
            stack.push(i);
            // find new neigbours recursively until we find all particles in the component
            while (!stack.empty()) {
                const Size index = stack.pop();
                finder->findNeighbours(index, r[index][H] * radius, neighs);
                for (auto& n : neighs) {
                    if (separateFlag && flag[index] != flag[n.index]) {
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

static Array<Float> getBodiesRadii(const Storage& storage,
    const Post::HistogramParams::Source source,
    const Post::HistogramParams::Quantity quantity) {
    switch (source) {
    case Post::HistogramParams::Source::PARTICLES: {
        Array<Float> values(storage.getParticleCnt());
        switch (quantity) {
        case Post::HistogramParams::Quantity::RADII: {
            ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
            for (Size i = 0; i < r.size(); ++i) {
                values[i] = r[i][H];
            }
            break;
        }
        case Post::HistogramParams::Quantity::VELOCITIES: {
            ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITIONS);
            for (Size i = 0; i < v.size(); ++i) {
                values[i] = getLength(v[i]);
            }
            break;
        }
        }
        std::sort(values.begin(), values.end());
        return values;
    }
    case Post::HistogramParams::Source::COMPONENTS: {
        Array<Size> components;
        const Size numComponents =
            findComponents(storage, RunSettings::getDefaults(), Post::ComponentConnectivity::ANY, components);

        Array<Float> values(numComponents);
        values.fill(0._f);
        switch (quantity) {
        case Post::HistogramParams::Quantity::RADII: {
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
        case Post::HistogramParams::Quantity::VELOCITIES: {
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
    Array<Float> radii = getBodiesRadii(storage, params.source, params.quantity);

    Range range = params.range;
    if (range.isEmpty()) {
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
    Array<Float> radii = getBodiesRadii(storage, params.source, params.quantity);

    Range range = params.range;
    if (range == Range::unbounded()) {
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
    class DummyColumn : public Abstract::Column {
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
