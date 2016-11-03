#include "gui/glpane.h"

// include OpenGL
#ifdef __WXMAC__
#include "OpenGL/gl.h"
#include "OpenGL/glu.h"
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

NAMESPACE_GUI_BEGIN

enum { ID_REPAINT = 1, ID_RELOAD = 2 };

BEGIN_EVENT_TABLE(CustomGlPane, wxGLCanvas)
    /*    EVT_MOTION(CustomGlPane::mouseMoved)
        EVT_LEFT_DOWN(CustomGlPane::mouseDown)
        EVT_LEFT_UP(CustomGlPane::mouseReleased)
        EVT_RIGHT_DOWN(CustomGlPane::rightClick)
        EVT_LEAVE_WINDOW(CustomGlPane::mouseLeftWindow)*/
    EVT_SIZE(CustomGlPane::resized)
    /*    EVT_KEY_DOWN(CustomGlPane::keyPressed)
        EVT_KEY_UP(CustomGlPane::keyReleased)
        EVT_MOUSEWHEEL(CustomGlPane::mouseWheelMoved)*/
    EVT_PAINT(CustomGlPane::render)
    EVT_TIMER(ID_REPAINT, CustomGlPane::onTimer)
    EVT_TIMER(ID_RELOAD, CustomGlPane::onTimer)
END_EVENT_TABLE()


// some useful events to use
/*void CustomGlPane::mouseMoved(wxMouseEvent& evt) {}
void CustomGlPane::mouseDown(wxMouseEvent& evt) {}
void CustomGlPane::mouseWheelMoved(wxMouseEvent& evt) {}
void CustomGlPane::mouseReleased(wxMouseEvent& evt) {}
void CustomGlPane::rightClick(wxMouseEvent& evt) {}
void CustomGlPane::mouseLeftWindow(wxMouseEvent& evt) {}
void CustomGlPane::keyPressed(wxKeyEvent& evt) {}
void CustomGlPane::keyReleased(wxKeyEvent& evt) {}*/

VisualSphere::VisualSphere(const int latitudeSegments, const int longitudeSegments) {
    const float dlon   = 2 * Math::PI / float(longitudeSegments) + EPS;
    const float dlat   = Math::PI / float(latitudeSegments) + EPS;
    const float radius = 1.f;
    // north pole vertex
    const Vector np = spherical(radius, 0.f, 0.f);
    vertices.push(np);
    normals.push(np);
    // all middle vertices
    for (float lat = dlat; lat < Math::PI; lat += dlat) {
        for (float lon = 0.f; lon < 2 * Math::PI; lon += dlon) {
            const Vector r = spherical(radius, lat, lon);
            vertices.push(r);
            normals.push(r);
        }
    }
    // south pole vertex
    const Vector sp = spherical(radius, Math::PI, 0.f);
    vertices.push(sp);
    normals.push(sp);

    auto map = [latitudeSegments, longitudeSegments](const int i, const int j) {
        const int offset = (i % longitudeSegments) + j * longitudeSegments;
        return offset + 1;
    };

    // north pole triangles
    for (int i = 0; i < longitudeSegments; ++i) {
        indices.pushAll({ 0, i + 1, (i + 1) % longitudeSegments + 1 });
    }
    // all middle triangles (quads)
    for (int j = 0; j < latitudeSegments - 2; ++j) {
        for (int i = 0; i < longitudeSegments; ++i) {
            indices.pushAll({ map(i, j), map(i + 1, j + 1), map(i + 1, j) });
            indices.pushAll({ map(i, j), map(i, j + 1), map(i + 1, j + 1) });
        }
    }
    // south pole triangles
    const int j     = latitudeSegments - 2;
    const int spIdx = vertices.size() - 1;
    for (int i = 0; i < longitudeSegments; ++i) {
        indices.pushAll({ spIdx, map(i + 1, j), map(i, j) });
    }
}

void VisualSphere::push(const Vector& center,
                        const float radius,
                        Array<Vector>& vs,
                        Array<Vector>& ns,
                        Array<int>& is) const {
    int begVecIdx  = vs.size();
    int begIdcsIdx = is.size();
    vs.pushAll(vertices);
    ns.pushAll(normals);
    is.pushAll(indices);

    // move and scale vertices
    for (int i = begVecIdx; i < vs.size(); ++i) {
        vs[i] = vs[i] * radius + center;
    }
    // increase indices
    for (int i = begIdcsIdx; i < is.size(); ++i) {
        is[i] += begVecIdx;
    }
}

void CustomGlPane::onTimer(wxTimerEvent& evt) {
    switch (evt.GetId()) {
    case ID_REPAINT:
        this->Refresh();
        break;
    case ID_RELOAD:
        if (!cached.positions.empty()) {
            vertices.clear();
            normals.clear();
            indices.clear();
            for (const Vector& v : cached.positions) {
                // drawSphere(v, v[H], 5, 7, vertices, normals, indices);
                sphere.push(v, v[H], vertices, normals, indices);
            }
            // std::cout << "vertices: " << vertices.size() << "  indices: " << indices.size()
            //        << "   spheres: " << cached.positions.size() << std::endl;
        }
        break;
    }
}

void CustomGlPane::draw(ArrayView<const Vector> positions) {
    cached.positions = positions;
    /// \todo sharing data between different threads will break something sooner or later ...
}

CustomGlPane::CustomGlPane(wxFrame* parent, int* args)
    : wxGLCanvas(parent, wxID_ANY, args, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
    , sphere(5, 7) {
    context = std::make_unique<wxGLContext>(this);


    // To avoid flashing on MSW
    SetBackgroundStyle(wxBG_STYLE_CUSTOM);

    reloadTimer = new wxTimer(this, ID_RELOAD);
    reloadTimer->Start(1000);

    repaintTimer = new wxTimer(this, ID_REPAINT);
    repaintTimer->Start(50);
}

CustomGlPane::~CustomGlPane() {}

void CustomGlPane::resized(wxSizeEvent& evt) {
    //	wxGLCanvas::OnSize(evt);
    Refresh();

    prepare3DViewport(0, 0, getWidth(), getHeight());


    evt.Skip();
}

/** Inits the OpenGL viewport for drawing in 3D. */
void CustomGlPane::prepare3DViewport(int topleft_x, int topleft_y, int bottomrigth_x, int bottomrigth_y) {

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black Background
    glClearDepth(1.0f);                   // Depth Buffer Setup
    glEnable(GL_DEPTH_TEST);              // Enables Depth Testing
    glDepthFunc(GL_LEQUAL);               // The Type Of Depth Testing To Do
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

    glEnable(GL_COLOR_MATERIAL);

    glEnable(GL_CULL_FACE);
    glEnable(GL_NORMALIZE);

    GLfloat light_ambient[]  = { 0.1, 0.1, 0.1, 1.0 };
    GLfloat light_diffuse[]  = { 0.7, 0.7, 0.7, 1.0 };
    GLfloat light_position[] = { 0.0, 0.0, 100.0, 0.0 };

    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);

    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHTING);

    glViewport(topleft_x, topleft_y, bottomrigth_x - topleft_x, bottomrigth_y - topleft_y);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float ratio_w_h = (float)(bottomrigth_x - topleft_x) / (float)(bottomrigth_y - topleft_y);
    gluPerspective(45, ratio_w_h, 0.1, 200.f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor4f(0.7f, 0.7f, 0.7f, 1);
}

int CustomGlPane::getWidth() {
    return GetSize().x;
}

int CustomGlPane::getHeight() {
    return GetSize().y;
}

void CustomGlPane::render(wxPaintEvent& UNUSED(evt)) {
    if (!IsShown()) {
        return;
    }

    wxGLCanvas::SetCurrent(*context);
    wxPaintDC(this); // only to be used in paint events. use wxClientDC to paint outside the paint event

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glLoadIdentity();

    glTranslatef(0.f, 0.f, -5.f);
    glRotatef(rotate, 0.3f, 1.0f, 0.0f);
    rotate += 4.f;

    if (!vertices.empty()) {

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        glEnableClientState(GL_INDEX_ARRAY);

        glVertexPointer(3, GL_FLOAT, sizeof(Vector), &vertices[0]);
        glNormalPointer(GL_FLOAT, sizeof(Vector), &normals[0]);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, &indices[0]);
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_INDEX_ARRAY);
    }

    glFlush();
    SwapBuffers();
}

NAMESPACE_GUI_END
