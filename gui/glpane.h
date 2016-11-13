#pragma once

#include "wx/glcanvas.h"
#include "wx/timer.h"
#include "wx/wx.h"

#include "gui/common.h"
#include "objects/containers/BufferedArray.h"
#include <thread>

class GLUquadric;

NAMESPACE_GUI_BEGIN

/// Unit sphere
class VisualSphere : public Object {
private:
    Array<Vector> vertices;
    Array<Vector> normals;
    Array<int> indices;

public:
    VisualSphere(const int latitudeSegments, const int longitudeSegments);

    void push(const Vector& center,
              const float radius,
              Array<Vector>& vs,
              Array<Vector>& ns,
              Array<int>& is) const;
};

class CustomGlPane : public wxGLCanvas {
private:
    std::unique_ptr<wxGLContext> context;
    float rotate = 0.f;
    wxTimer* repaintTimer;
    wxTimer* reloadTimer;

    struct {
        ArrayView<const Vector> positions;
    } cached;

    GLUquadric* quadric;

    std::thread reloadThread;
    BufferedArray<Vector> vertices;
    BufferedArray<Vector> normals;
    BufferedArray<int> indices;

    VisualSphere sphere;

public:
    CustomGlPane(wxFrame* parent, int* args);
    virtual ~CustomGlPane();

    void resized(wxSizeEvent& evt);

    int getWidth();
    int getHeight();

    void render(wxPaintEvent& evt);
    void prepare3DViewport(int topleft_x, int topleft_y, int bottomrigth_x, int bottomrigth_y);
    void prepare2DViewport(int topleft_x, int topleft_y, int bottomrigth_x, int bottomrigth_y);

    // events
    void mouseMoved(wxMouseEvent& event);
    void mouseDown(wxMouseEvent& event);
    void mouseWheelMoved(wxMouseEvent& event);
    void mouseReleased(wxMouseEvent& event);
    void rightClick(wxMouseEvent& event);
    void mouseLeftWindow(wxMouseEvent& event);
    void keyPressed(wxKeyEvent& event);
    void keyReleased(wxKeyEvent& event);

    void onTimer(wxTimerEvent& evt);

    void draw(ArrayView<const Vector> positions);

    void drawSphere(const Vector& position,
                    const float radius,
                    const int latitudeSegments,
                    const int longitudeSegments,
                    Array<Vector>& vertices,
                    Array<Vector>& normals,
                    Array<int>& indices);

    DECLARE_EVENT_TABLE()
};

NAMESPACE_GUI_END
