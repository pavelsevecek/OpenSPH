#pragma once

#include "gui/Common.h"
#include "gui/objects/Point.h"
#include "quantities/Storage.h"


NAMESPACE_SPH_BEGIN

class Bitmap;
namespace Abstract {
    class Camera;
}


/// Partially overlaps with GuiSettings, but it's better to have render specific settings in one struct than
/// one huge catch-all settings.
struct RenderParams {
    /// Resolution of the produced bitmap
    Point size = Point(640, 480);

    struct Particle {
        /// Scaling factor of drawn particles relative to 1. Can be (in theory) any positive value.
        float scale = 1.f;
    } particles;

    /// Clone used buffers before rendering
    bool clone;
};

namespace Abstract {
    class Element;

    class Renderer : public Polymorphic {
    public:
        /// Draws particles into the bitmap. Implementation shall be callable from any thread, but does not
        /// have to be thread-safe (never will be executed from multiple threads at once)
        /// \param positions Particle positions, must match the particles in element.
        /// \param element Data-to-color conversion object for particles. Must be already initialized!
        /// \param params Parameters of the render
        /// \param stats Input-output parameter, contains run statistics that can be included in the render
        ///              (run time, timestep, ...), renderers can also output some statistics of their own
        ///              (time used in rendering, framerate, ...)
        virtual Bitmap render(ArrayView<const Vector> positions,
            const Abstract::Element& element,
            const Abstract::Camera& camera,
            const RenderParams& params,
            Statistics& stats) const = 0;
    };
}

NAMESPACE_SPH_END
