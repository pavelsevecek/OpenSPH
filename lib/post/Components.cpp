#include "post/Components.h"
#include "quantities/Storage.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

Size findComponents(ArrayView<const Vector> vertices, const GlobalSettings& settings, Array<Size>& indices) {
    indices.resize(vertices.size());
    const Size unassigned = std::numeric_limits<Size>::max();
    indices.fill(unassigned);
    Size componentIdx = 0;
    std::unique_ptr<Abstract::Finder> finder = Factory::getFinder(settings);
    finder->build(vertices);
    const Float radius = Factory::getKernel<3>(settings).radius();

    Array<Size> stack;
    Array<NeighbourRecord> neighs;

    for (Size i = 0; i < vertices.size(); ++i) {
        if (indices[i] == unassigned) {
            indices[i] = componentIdx;
            stack.push(i);
            while (!stack.empty()) {
                const int index = stack.pop();
                finder->findNeighbours(index, vertices[index][H] * radius, neighs);
                for (auto& n : neighs) {
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

Array<Size> getCummulativeSFD(Storage& storage,
    const GlobalSettings& settings,
    Optional<HistogramParams> params) {
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityKey::POSITIONS);
    Array<Size> components;
    const Size numComponents = findComponents(r, settings, components);
    Array<Float> volumes(numComponents);
    volumes.fill(0._f);
    ArrayView<Float> rho, m;
    tie(rho, m) = storage.getValues<Float>(QuantityKey::DENSITY, QuantityKey::MASSES);
    for (Size i = 0; i < r.size(); ++i) {
        volumes[components[i]] += m[i] / rho[i];
    }
    Range range = params ? params->range : [&]() {
        Extended minVolume(Extended::infinity()), maxVolume(-Extended::infinity());
        for (Float v : volumes) {
            minVolume = min(minVolume, Extended(v));
            maxVolume = max(maxVolume, Extended(v));
        }
        return Range(minVolume, maxVolume);
    }();
    // estimate initial bin count as sqrt of component count
    Size binCnt = params ? params->binCnt : Size(sqrt(components.size()));
    Array<Size> histogram(binCnt);
    histogram.fill(0);
    ASSERT(binCnt > 0);
    for (Float v : volumes) {
        const Size idx = (binCnt - 1) * Float((Extended(v) - range.lower()) / range.size());
        histogram[idx]++;
    }
    /// \todo check histogram, if some bins are "oversampled", increase bin cnt

    /// \todo diameters, not volumes
    // sum up histogram
    for (Size i = 0; i < histogram.size() - 1; ++i) {
        histogram[i + 1] += histogram[i];
    }
    // how to return range? That's not containted in histogram
    return histogram;
}

NAMESPACE_SPH_END
