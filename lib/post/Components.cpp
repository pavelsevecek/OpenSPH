#include "post/Components.h"
#include "geometry/Box.h"
#include "post/MarchingCubes.h"
#include "quantities/Storage.h"
#include "sph/kernel/KernelFactory.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

Size findComponents(ArrayView<const Vector> vertices, const RunSettings& settings, Array<Size>& indices) {
    indices.resize(vertices.size());
    const Size unassigned = std::numeric_limits<Size>::max();
    indices.fill(unassigned);
    Size componentIdx = 0;
    AutoPtr<Abstract::Finder> finder = Factory::getFinder(settings);
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

struct NumberDensityField : public Abstract::FieldFunction {
private:
    LutKernel<3>& kernel;
    Abstract::Finder& finder;

    ArrayView<const Vector> r;
    ArrayView<const Float> m, rho;
    Array<NeighbourRecord> neighs;

public:
    Float maxH = 0._f;

    NumberDensityField(const Storage& storage,
        const ArrayView<const Vector> r,
        LutKernel<3>& kernel,
        Abstract::Finder& finder)
        : kernel(kernel)
        , finder(finder)
        , r(r) {
        ArrayView<const Float> m, rho;
        tie(m, rho) = storage.getValues<Float>(QuantityId::MASSES, QuantityId::DENSITY);

        // we have to re-build the tree since we are using different positions (in general)
        finder.build(r);
    }

    virtual Float operator()(const Vector& pos) override {
        ASSERT(maxH > 0._f);
        finder.findNeighbours(pos, maxH * kernel.radius(), neighs);
        Float phi = 0._f;

        // find average h of neighbours
        Float avgH = 0._f;
        for (NeighbourRecord& n : neighs) {
            avgH += r[n.index][H];
        }
        avgH /= neighs.size();

        // interpolate values of neighbours
        for (NeighbourRecord& n : neighs) {
            const Size j = n.index;
            phi += m[j] / rho[j] * kernel.value(pos - r[j], avgH);
        }
        return phi;
    }
};

Array<Triangle> getSurfaceMesh(const Storage& storage, const Float gridResolution, const Float surfaceLevel) {
    // http://www.cc.gatech.edu/~turk/my_papers/sph_surfaces.pdf

    // 1. get denoised particle positions
    const Float lambda = 0.9_f;
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    Array<Vector> r_bar(r.size());
    RunSettings settings;
    LutKernel<3> kernel = Factory::getKernel<3>(settings);
    AutoPtr<Abstract::Finder> finder = Factory::getFinder(settings);
    Array<NeighbourRecord> neighs;
    for (Size i = 0; i < r.size(); ++i) {
        finder->findNeighbours(i, r[i][H] * kernel.radius(), neighs);
        Vector wr(0._f);
        Float wsum = 0._f;
        for (NeighbourRecord& n : neighs) {
            const Size j = n.index;
            const Float w = kernel.value(r[i] - r[j], r[i][H]);
            wr += w * r[j];
            wsum += w;
        }
        // Eq. (6) from the paper
        r_bar[i] = (1._f - lambda) * r[i] + lambda * wr / wsum;
    }

    /// \todo we skip the anisotropy correction for now

    // 2. find bounding box and maximum h of each component
    Array<Size> indices;
    const Size componentCnt = findComponents(r_bar, settings, indices);
    Array<Box> boxes(componentCnt);
    Array<Float> maxH(componentCnt);
    maxH.fill(0._f);
    for (Size i = 0; i < r_bar.size(); ++i) {
        const Size j = indices[i];
        boxes[j].extend(r_bar[i]);
        maxH[j] = max(maxH[j], r_bar[i][H]); // h should be interpolate just like vectors r
    }

    AutoPtr<NumberDensityField> field = makeAuto<NumberDensityField>(storage, r_bar, kernel, *finder);
    MarchingCubes mc(r_bar, surfaceLevel, std::move(field));

    // 3. find surface of each component
    for (Size componentIdx = 0; componentIdx < componentCnt; ++componentIdx) {
        field->maxH = maxH[componentIdx];
        mc.addComponent(boxes[componentIdx], gridResolution);
    }

    return std::move(mc.getTriangles());
}

Array<Size> getDifferentialSFD(Storage& storage,
    const RunSettings& settings,
    Optional<HistogramParams> params) {
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    Array<Size> components;
    const Size numComponents = findComponents(r, settings, components);
    Array<Float> volumes(numComponents);
    volumes.fill(0._f);
    ArrayView<Float> rho, m;
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
