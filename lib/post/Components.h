#pragma once

#include "objects/finders/AbstractFinder.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

class Storage;
class Triangle;

/// Finds and marks connected components (a.k.a. separated bodies) in the array of vertices.
/// \return Array of indices from 0 to n-1, where n is the number of components. In the array, i-th index
///         corresponds to component to which i-th particle belongs.
/// \todo generalize particle connectivity, for example do not count damaged particles.
Size findComponents(ArrayView<const Vector> vertices, const RunSettings& settings, Array<Size>& indices);


/// Returns the triangle mesh of the body surface (or surfaces of bodies).
/// \param storage Particle storage; must contain particle positions and neighbour counts.
/// \param surfaceLevel (Number) density defining the surface. Higher value is more likely to cause SPH
///                     particles being separated into smaller groups (droplets), lower value will cause the
///                     boundary to be "bulgy" rather than smooth
Array<Triangle> getSurfaceMesh(const Storage& storage, const Float gridResolution, const Float surfaceLevel);

/// Parameters of the histogram
struct HistogramParams {
    Range range; ///< Range of values from which the histogram is constructed.
    Size binCnt; ///< Number of histogram bins, must be at least 1.
};

/// Computes cummulative size-frequency distribution of body sizes (equivalent diameters) in storage.
/// The storage must contain at least particle positions, masses and densities.
/// \param params Parameters of the histogram, by default they are deduced from input data, meaning the range
///        is given by the minimum and maximum value and the bin count is selected as a compromise between
///        number of values per bin and histogram resolution.
Array<Size> getCummulativeSFD(Storage& storage,
    const RunSettings& settings,
    Optional<HistogramParams> params = NOTHING);

NAMESPACE_SPH_END
