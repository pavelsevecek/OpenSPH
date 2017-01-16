#pragma once

#include "wx/glcanvas.h"
#include "wx/timer.h"
#include "wx/wx.h"

#include "gui/Common.h"
#include "gui/Renderer.h"
#include "objects/containers/BufferedArray.h"
#include <thread>

class GLUquadric;

NAMESPACE_SPH_BEGIN

/// Unit sphere
class VisualSphere  {
private:
    Array<Vector> vertices;
    Array<Vector> normals;
    Array<Size> indices;

public:
    VisualSphere(const Size latitudeSegments, const Size longitudeSegments);

    void push(const Vector& center,
              const float radius,
              Array<Vector>& vs,
              Array<Vector>& ns,
              Array<Size>& is) const;
};

class CustomGlPane : public wxGLCanvas, public Abstract::Renderer {
private:
    std::unique_ptr<wxGLContext> context;
    float rotate = 0.f;
    wxTimer* repaintTimer;
    wxTimer* reloadTimer;

    struct {
        Array<Vector> positions;
    } cached;

    GLUquadric* quadric;

    std::thread reloadThread;
    BufferedArray<Vector> vertices;
    BufferedArray<Vector> normals;
    BufferedArray<Size> indices;

    VisualSphere sphere;

public:
    CustomGlPane(wxFrame* parent, Array<int> args);

    void resized(wxSizeEvent& evt);

    int getWidth();
    int getHeight();

    void render(wxPaintEvent& evt);
    void prepare3DViewport(int topleft_x, int topleft_y, int bottomrigth_x, int bottomrigth_y);
    void prepare2DViewport(int topleft_x, int topleft_y, int bottomrigth_x, int bottomrigth_y);

    void onTimer(wxTimerEvent& evt);

    virtual void draw(const std::shared_ptr<Storage>& storage) override;

    virtual void setQuantity(const QuantityIds key) override;

    void drawSphere(const Vector& position,
                    const float radius,
                    const int latitudeSegments,
                    const int longitudeSegments,
                    Array<Vector>& vertices,
                    Array<Vector>& normals,
                    Array<int>& indices);
};

NAMESPACE_SPH_END
