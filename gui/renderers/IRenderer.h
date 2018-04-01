#pragma once

/// \file IRenderer.h
/// \brief Interface for renderers
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "gui/Utils.h"
#include "gui/objects/Point.h"
#include "quantities/Particle.h"

NAMESPACE_SPH_BEGIN

class Bitmap;
class ICamera;
class IColorizer;

/// \brief Parameters of the rendered image
///
/// Partially overlaps with GuiSettings, but it's better to have render specific settings in one struct than
/// one huge catch-all settings.
struct RenderParams {

    /// Resolution of the produced bitmap
    Point size = Point(640, 480);

    struct {
        /// Scaling factor of drawn particles relative to 1. Can be (in theory) any positive value.
        float scale = 1.f;

    } particles;

    struct {
        /// Multiplier of the rendered vector field
        float scale = 1.e4_f;

        /// Size of the vector proportional to the logarithm of velocity
        bool toLog = true;

    } vectors;

    /// Highlighted particle (only for interactive view). If NOTHING, no particle is selected.
    Optional<Size> selectedParticle = NOTHING;
};


class IRenderer : public Polymorphic {
public:
    /// Prepares the objects for rendering and updates its data. Called every time a parameter change.
    /// \param storage Storage containing positions of particles, must match the particles in colorizer.
    /// \param colorizer Data-to-color conversion object for particles. Must be already initialized!
    /// \param camera Camera used for rendering.
    virtual void initialize(const Storage& storage, const IColorizer& colorizer, const ICamera& camera) = 0;

    /// \brief Draws particles into the bitmap, given the data provided in \ref initialize.
    ///
    /// This function is called every time the view changes (display parameters change, camera pan & zoom,
    /// ...). Implementation shall be callable from any thread, but does not have to be thread-safe (never
    /// will be executed from multiple threads at once).
    /// \param camera Camera used for rendering
    /// \param params Parameters of the render
    /// \param stats Input-output parameter, contains run statistics that can be included in the render
    ///              (run time, timestep, ...), renderers can also output some statistics of their own
    ///              (time used in rendering, framerate, ...)
    virtual SharedPtr<wxBitmap> render(const ICamera& camera,
        const RenderParams& params,
        Statistics& stats) const = 0;
};

NAMESPACE_SPH_END
