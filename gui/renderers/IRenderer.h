#pragma once

/// \file IRenderer.h
/// \brief Interface for renderers
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "gui/objects/Color.h"
#include "gui/objects/Point.h"
#include "objects/wrappers/ClonePtr.h"
#include "objects/wrappers/Flags.h"
#include "quantities/Particle.h"

NAMESPACE_SPH_BEGIN

template <typename T>
class Bitmap;
class ICamera;
class IColorizer;
class Statistics;

enum class TextAlign {
    LEFT = 1 << 0,
    RIGHT = 1 << 1,
    HORIZONTAL_CENTER = 1 << 2,
    TOP = 1 << 3,
    BOTTOM = 1 << 4,
    VERTICAL_CENTER = 1 << 5,
};

class IRenderOutput : public Polymorphic {
public:
    /// \todo would be nice to make it more general, cleaner
    struct Label {
        std::wstring text;
        Rgba color;
        int fontSize;
        Flags<TextAlign> align;
        Pixel position;
    };

    /// May be called once after render finishes or multiple times for progressive renderers.
    virtual void update(const Bitmap<Rgba>& bitmap, Array<Label>&& labels) = 0;
};


/// \brief Parameters of the rendered image
///
/// Partially overlaps with GuiSettings, but it's better to have render specific settings in one struct than
/// one huge catch-all settings.
struct RenderParams {

    /// \brief Resolution of the produced bitmap
    Pixel size = Pixel(640, 480);

    /// \brief Camera used for rendering.
    AutoPtr<ICamera> camera;

    /// \brief Parameters of the particle renderer
    struct {
        /// \brief  Scaling factor of drawn particles relative to 1.
        ///
        /// Can be (in theory) any positive value.
        float scale = 1.f;

        /// \brief Highlighted particle (only for interactive view).
        ///
        /// If NOTHING, no particle is selected.
        Optional<Size> selected = NOTHING;

        /// \brief If true, the palette is converted to grayscale.
        bool grayScale = false;

        /// \brief If true, the particles will be drawn with antialiasing.
        ///
        /// This will generally improve quality, but may slow down the rendering.
        bool doAntialiasing = false;

        /// \brief If true, particles will be smoothed using cubic spline.
        ///
        /// Only used if doAntialiasing is true.
        bool smoothed = false;

        /// \brief If true, a color palette and a distance scale is included in the image.
        bool showKey = true;

    } particles;

    /// \brief Parameters of rendered vectors
    struct {

        /// \brief Length of the drawn vectors in pixels;
        float length = 100.f;

    } vectors;

    /// \brief Parameters of rendered surface
    struct {

        /// \brief Value of the iso-surface defining the rendered surface
        float level = 0.15f;

        /// \brief Intensity of the ambient light, illuminating every point unconditionally.
        Float ambientLight = 0.3f;

        /// \brief Intensity of the sunlight.
        Float sunLight = 0.7f;

    } surface;
};

/// \brief Interface used to implement renderers.
class IRenderer : public Polymorphic {
public:
    /// \brief Prepares the objects for rendering and updates its data.
    ///
    /// Called every time a parameter changes. Renderer should cache any data necessary for rendering of
    /// particles (particle positions, colors, etc.).
    /// \param storage Storage containing positions of particles, must match the particles in colorizer.
    /// \param colorizer Data-to-color conversion object for particles. Must be already initialized!
    /// \param camera Camera used for rendering.
    virtual void initialize(const Storage& storage, const IColorizer& colorizer, const ICamera& camera) = 0;

    /// \brief Draws particles into the bitmap, given the data provided in \ref initialize.
    ///
    /// This function is called every time the view changes (display parameters change, camera pan & zoom,
    /// ...). Implementation shall be callable from any thread, but does not have to be thread-safe (never
    /// will be executed from multiple threads at once).
    /// \param params Parameters of the render
    /// \param stats Input-output parameter, contains run statistics that can be included in the render
    ///              (run time, timestep, ...), renderers can also output some statistics of their own
    ///              (time used in rendering, framerate, ...)
    virtual void render(const RenderParams& params, Statistics& stats, IRenderOutput& output) const = 0;

    /// \todo
    virtual void cancelRender() = 0;
};

NAMESPACE_SPH_END
