#pragma once

#include "objects/finders/AbstractFinder.h"
#include "quantities/Storage.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

/// Finds and marks connected components (a.k.a. separated bodies) in the array of vertices.
/// \return Array of indices from 0 to n-1, where n is the number of components. In the array, i-th index
///         corresponds to component to which i-th particle belongs.
/// \todo generalize particle connectivity, for example do not count damaged particles
inline Array<Size> findComponents(ArrayView<const Vector> vertices, const GlobalSettings& settings) {
    Array<Size> indices(vertices.size());
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
    return indices;
}

NAMESPACE_SPH_END
