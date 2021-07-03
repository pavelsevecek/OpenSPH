#pragma once

/// \file IRenderer.h
/// \brief Interface for renderers
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "gui/objects/Color.h"
#include "gui/objects/Point.h"
#include "gui/objects/Texture.h"
#include "objects/wrappers/Any.h"
#include "objects/wrappers/Flags.h"
#include "quantities/Particle.h"
#include "thread/ThreadLocal.h"
#include <atomic>

NAMESPACE_SPH_BEGIN

class ICamera;
class CameraRay;
class ITracker;
class IColorizer;
class IColorMap;
class FrameBuffer;
class Statistics;
class GuiSettings;

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
    virtual void update(const Bitmap<Rgba>& bitmap, Array<Label>&& labels, const bool isFinal) = 0;
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

    /// \brief Tracker used for camera motion.
    ///
    /// May be nullptr for static camera.
    AutoPtr<ITracker> tracker;

    /// \brief Background color of the rendered image.
    Rgba background = Rgba::black();

    /// \brief If true, a color palette and a distance scale is included in the image.
    bool showKey = true;

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

        /// \brief If true, ghost particles (if present) will be rendered as empty circles.
        bool renderGhosts = true;

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
        float ambientLight = 0.3f;

        /// \brief Intensity of the sunlight.
        float sunLight = 0.7f;

        /// \brief Emission multiplier
        float emission = 1.f;

        /// \brief Width of the image reconstruction filter
        float filterWidth = 2.f;

    } surface;

    /// \brief Parameters of volumetric renderer
    struct {
        /// \brief Emission per unit distance [m^-1]
        float emission = 1.f;
    } volume;

    struct {

        /// \brief Step between subsequent iso-lines
        float isoStep = 30.f;

        /// \brief Horizontal resolution of the grid
        Size gridSize = 100;

        /// \brief Show numerical values of iso-lines
        bool showLabels = true;

    } contours;

    /// \brief Sets up parameters using values stored in settings.
    ///
    /// This does NOT initialize camera and resolution of the render.
    void initialize(const GuiSettings& gui);
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

    /// \brief Stops the rendering if it is currently in progress.
    virtual void cancelRender() = 0;
};

/// \brief Base class for renderers based on raytracing
class IRaytracer : public IRenderer {
protected:
    SharedPtr<IScheduler> scheduler;

    struct ThreadData {
        /// Random-number generator for this thread.
        UniformRng rng;

        /// Additional data used by the implementation
        Any data;

        ThreadData(int seed);
    };

    mutable ThreadLocal<ThreadData> threadData;

    mutable std::atomic_bool shouldContinue;

private:
    /// \brief Parameters fixed for the renderer.
    ///
    /// Note that additional parameters which can differ for each rendered image are passed to \ref render.
    struct {

        /// Color mapping operator
        AutoPtr<IColorMap> colorMap;

        struct {

            Rgba color = Rgba::black();

            /// HDRI for the background. Can be empty.
            Texture hdri;

        } enviro;

        /// Number of iterations of the progressive renderer.
        Size iterationLimit;

        /// Number of subsampled iterations.
        Size subsampling;

    } fixed;

public:
    IRaytracer(SharedPtr<IScheduler> scheduler, const GuiSettings& gui);

    virtual void render(const RenderParams& params, Statistics& stats, IRenderOutput& output) const final;

    virtual void cancelRender() override {
        shouldContinue = false;
    }

protected:
    virtual Rgba shade(const RenderParams& params, const CameraRay& ray, ThreadData& data) const = 0;

    Rgba getEnviroColor(const CameraRay& ray) const;

private:
    void refine(const RenderParams& params, const Size iteration, FrameBuffer& fb) const;
};

NAMESPACE_SPH_END
