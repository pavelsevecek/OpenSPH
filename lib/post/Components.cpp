#include "post/Components.h"
#include "objects/geometry/Box.h"
#include "post/MarchingCubes.h"
#include "quantities/Storage.h"
#include "sph/kernel/KernelFactory.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

Size findComponents(const Storage& storage,
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

Array<Size> getDifferentialSFD(const Storage& storage,
    const RunSettings& settings,
    Optional<HistogramParams> params) {
    Array<Size> components;
    const Size numComponents = findComponents(storage, settings, ComponentConnectivity::ANY, components);
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    Array<Float> volumes(numComponents);
    volumes.fill(0._f);
    ArrayView<const Float> rho, m;
    tie(rho, m) = storage.getValues<Float>(QuantityId::DENSITY, QuantityId::MASSES);
    for (Size i = 0; i < r.size(); ++i) {
        volumes[components[i]] += m[i] / rho[i];
    }
    Range range = params ? params->range : [&]() {
        Float minVolume = INFTY, maxVolume = -INFTY;
        for (Float v : volumes) {
            minVolume = min(minVolume, v);
            maxVolume = max(maxVolume, v);
        }
        return Range(minVolume, maxVolume);
    }();
    // estimate initial bin count as sqrt of component count
    Size binCnt = params ? params->binCnt : Size(sqrt(components.size()));
    Array<Size> histogram(binCnt);
    histogram.fill(0);
    ASSERT(binCnt > 0);
    for (Float v : volumes) {
        const Size idx = (binCnt - 1) * (v - range.lower()) / range.size();
        histogram[idx]++;
    }
    /// \todo check histogram, if some bins are "oversampled", increase bin cnt

    /// \todo diameters, not volumes
    // how to return range? That's not containted in histogram
    return histogram;
}

NAMESPACE_SPH_END
