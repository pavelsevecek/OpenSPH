#pragma once

#include "gui/ArcBall.h"
#include "gui/Settings.h"
#include "gui/objects/Point.h"
#include "gui/windows/IGraphicsPane.h"
#include "objects/wrappers/Function.h"
#include "objects/wrappers/Optional.h"
#include <wx/image.h>

NAMESPACE_SPH_BEGIN

class Controller;
class ICamera;

class OrthoPane : public IGraphicsPane {
private:
    enum class MeasurementMode {
        NONE,
        RULER,
        PARTICLE_PAIR,
    };

    struct ParticleAnchor {
        Vector position = Vector(0._f);
        Size index = 0;
        bool usePersistentIndex = false;
    };

    Controller* controller;

    /// Helper for rotation
    ArcBall arcBall;

    AutoPtr<ICamera> camera;

    struct {
        /// Cached last mouse position when dragging the window
        Pixel position;

        /// Camera rotation matrix when dragging started.
        AffineMatrix initialMatrix = AffineMatrix::identity();
    } dragging;

    struct {
        Optional<Size> lastIdx;
    } particle;

    struct {
        MeasurementMode mode = MeasurementMode::NONE;
        Optional<Vector> first;
        Optional<Vector> second;
        Optional<Vector> preview;
        Optional<ParticleAnchor> firstParticle;
        Optional<ParticleAnchor> secondParticle;
        Float unitScale = 1.e3_f;
        String unitLabel = "km";
        String text = "Distance: --";
    } ruler;

public:
    Function<void(const String&)> onRulerTextChanged;

    OrthoPane(wxWindow* parent, Controller* controller, const GuiSettings& gui);

    ~OrthoPane();

    virtual ICamera& getCamera() override {
        return *camera;
    }

    virtual void resetView() override;

    virtual void onTimeStep(const Storage& storage, const Statistics& stats) override;

    void setRulerEnabled(const bool enabled);

    void setParticlePairEnabled(const bool enabled);

    void setRulerUnits(const Float unitScale, const String& unitLabel);

    void centerView();

private:
    /// wx event handlers
    void onPaint(wxPaintEvent& evt);

    void onMouseMotion(wxMouseEvent& evt);

    void onLeftUp(wxMouseEvent& evt);

    void onRightDown(wxMouseEvent& evt);

    void onRightUp(wxMouseEvent& evt);

    void onMouseWheel(wxMouseEvent& evt);

    void onResize(wxSizeEvent& evt);

    Optional<Vector> getAnchor(const Pixel& position) const;

    Optional<Vector> getPointOnViewPlane(const Pixel& position) const;

    Optional<ParticleAnchor> getParticleAnchor(const Size index) const;

    Optional<Vector> resolveParticleAnchor(const Storage& storage, const ParticleAnchor& anchor) const;

    void updateParticleAnchor(const Storage& storage, Optional<ParticleAnchor>& anchor);

    void updateRulerText();

    void notifyRulerTextChanged() const;

    void drawShearingBox(wxImage& image) const;

    void drawRuler(wxImage& image) const;
};

NAMESPACE_SPH_END
