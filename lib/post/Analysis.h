#pragma once

/// \file Analysis.h
/// \brief Various function for interpretation of the results of a simulation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/finders/INeighbourFinder.h"
#include "objects/wrappers/Expected.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

class Storage;
class Path;

namespace Post {

    enum class ComponentConnectivity {
        ANY,              ///< Any two particles can belong into the same component
        SEPARATE_BY_FLAG, ///< Particles with different flags belong a priori to different components
    };

    /// \brief Finds and marks connected components (a.k.a. separated bodies) in the array of vertices.
    ///
    /// \param storage Storage containing the particles. Must also contain QuantityId::FLAG if
    ///                SEPARATE_BY_FLAG option is used.
    /// \param settings Run settings (currently only used to get SPH kernel)
    /// \param connectivity Defines additional conditions of particle separation (besides their distance).
    /// \param indices[out] Array of indices from 0 to n-1, where n is the number of components. In the array,
    ///                     i-th index corresponds to component to which i-th particle belongs.
    /// \return Number of components
    Size findComponents(const Storage& storage,
        const RunSettings& settings,
        const ComponentConnectivity connectivity,
        Array<Size>& indices);


    /// \brief Quantity from which the histogram is constructed
    ///
    /// Beside the listed options, any QuantityId can be used, by casting to HistogramId enum. All values of
    /// HistogramId has to be negative to avoid clashes.
    enum class HistogramId {
        /// Particle radii or equivalent radii of compoennts
        RADII = -1,

        /// Particle velocities
        VELOCITIES = -2,
    };

    /// Parameters of the histogram
    struct HistogramParams {

        /// \brief Source data used to construct the histogram.
        enum class Source {
            /// Equivalent radii of connected chunks of particles (SPH framework)
            COMPONENTS,

            /// Radii of individual particles, considering particles as spheres (N-body framework)
            PARTICLES,
        };

        Source source = Source::PARTICLES;

        HistogramId id = HistogramId::RADII;

        /// Range of values from which the histogram is constructed. Unbounded range means the range is
        /// selected based on the source data.
        Interval range;

        /// Number of histogram bins. 0 means the number is selected based on the source data. Used only by
        /// differential SFD.
        Size binCnt = 0;
    };

    /// Point in SFD
    struct SfdPoint {
        /// Radius (x coordinate in the plot)
        Float radius;

        /// Number of particles/components
        Size count;
    };

    /// \brief Computes differential size-frequency distribution of particle radii.
    Array<SfdPoint> getDifferentialSfd(const Storage& storage, const HistogramParams& params);

    /// \brief Computes cummulative size-frequency distribution of body sizes (equivalent diameters).
    ///
    /// The storage must contain at least particle positions, masses and densities.
    /// \param params Parameters of the histogram.
    Array<SfdPoint> getCummulativeSfd(const Storage& storage, const HistogramParams& params);


    /// \brief Parses the pkdgrav output file and creates a storage with quantities stored in the file.
    ///
    /// Pkdgrav output contains particle positions, their velocities, angular velocities and their masses.
    /// Parsed storage is constructed with these quantities, the radii of the spheres are saved as
    /// H-component
    /// (however the radius is NOT equal to the smoothing length).
    /// The particles are sorted in the storage according to their masses, in descending order. The
    /// largest
    /// remnant (if exists) or largest fragment is therefore particle with index 0.
    /// \param path Path to the file. The extension of the file shall be ".bt".
    /// \return Storage with created quantities or error message.
    Expected<Storage> parsePkdgravOutput(const Path& path);
}

NAMESPACE_SPH_END
