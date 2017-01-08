#include "gui/GlPane.h"

// include OpenGL
#ifdef __WXMAC__
#include "OpenGL/gl.h"
#include "OpenGL/glu.h"
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

NAMESPACE_SPH_BEGIN

enum { ID_REPAINT = 1, ID_RELOAD = 2 };

VisualSphere::VisualSphere(const Size latitudeSegments, const Size longitudeSegments) {
    const float dlon   = 2 * PI / float(longitudeSegments) + EPS;
    const float dlat   = PI / float(latitudeSegments) + EPS;
    const float radius = 1.f;
    // north pole vertex
    const Vector np = spherical(radius, 0.f, 0.f);
    vertices.push(np);
    normals.push(np);
    // all middle vertices
    for (float lat = dlat; lat < PI; lat += dlat) {
        for (float lon = 0.f; lon < 2 * PI; lon += dlon) {
            const Vector r = spherical(radius, lat, lon);
            vertices.push(r);
            normals.push(r);
        }
    }
    // south pole vertex
    const Vector sp = spherical(radius, PI, 0.f);
    vertices.push(sp);
    normals.push(sp);

    auto map = [latitudeSegments, longitudeSegments](const Size i, const Size j) {
        const Size offset = (i % longitudeSegments) + j * longitudeSegments;
        return offset + 1;
    };

    // north pole triangles
    for (Size i = 0; i < longitudeSegments; ++i) {
        indices.pushAll({ 0, i + 1, (i + 1) % longitudeSegments + 1 });
    }
    // all middle triangles (quads)
    for (Size j = 0; j < latitudeSegments - 2; ++j) {
        for (Size i = 0; i < longitudeSegments; ++i) {
            indices.pushAll({ map(i, j), map(i + 1, j + 1), map(i + 1, j) });
            indices.pushAll({ map(i, j), map(i, j + 1), map(i + 1, j + 1) });
        }
    }
    // south pole triangles
    const Size j     = latitudeSegments - 2;
    const Size spIdx = vertices.size() - 1;
    for (Size i = 0; i < longitudeSegments; ++i) {
        indices.pushAll({ spIdx, map(i + 1, j), map(i, j) });
    }
}

void VisualSphere::push(const Vector& center,
                        const float radius,
                        Array<Vector>& vs,
                        Array<Vector>& ns,
                        Array<Size>& is) const {
    Size begVecIdx  = vs.size();
    Size begIdcsIdx = is.size();
    vs.pushAll(vertices);
    ns.pushAll(normals);
    is.pushAll(indices);

    // move and scale vertices
    for (Size i = begVecIdx; i < vs.size(); ++i) {
        vs[i] = vs[i] * radius + center;
    }
    for (Size i = begVecIdx; i < vs.size(); ++i) {
        ns[i] = getNormalized(vs[i]);
    }
    // increase indices
    for (Size i = begIdcsIdx; i < is.size(); ++i) {
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
            if (reloadThread.joinable()) {
                reloadThread.join();
            }
            reloadThread = std::thread([this]() {
                vertices->clear();
                normals->clear();
                indices->clear();
                for (const Vector& v : cached.positions) {
                    // drawSphere(v, v[H], 5, 7, vertices, normals, indices);
                    sphere.push(v, v[H], vertices.first(), normals.first(), indices.first());
                }
                vertices.swap();
                normals.swap();
                indices.swap();
            });
        }
        break;
    }
}

void CustomGlPane::draw(const std::shared_ptr<Storage>& storage) {
    cached.positions.clear();
    Array<Vector>& newPositions = storage->getValue<Vector>(QuantityKey::POSITIONS);
    cached.positions.pushAll(newPositions);
}

void CustomGlPane::setQuantity(const QuantityKey UNUSED(key)) {
    NOT_IMPLEMENTED;
}

CustomGlPane::CustomGlPane(wxFrame* parent, Array<int> args)
    : wxGLCanvas(parent, wxID_ANY, &args[0], wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
    , sphere(7, 9) {
    context = std::make_unique<wxGLContext>(this);


    // To avoid flashing on MSW
    SetBackgroundStyle(wxBG_STYLE_CUSTOM);

    reloadTimer = new wxTimer(this, ID_RELOAD);
    reloadTimer->Start(50);

    repaintTimer = new wxTimer(this, ID_REPAINT);
    repaintTimer->Start(20);
}

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
    // rotate += 4.f;

    /// draw spheres using buffered array
    if (!vertices.second().empty()) {

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        glEnableClientState(GL_INDEX_ARRAY);

        glVertexPointer(3, GL_FLOAT, sizeof(Vector), &vertices.second()[0]);
        glNormalPointer(GL_FLOAT, sizeof(Vector), &normals.second()[0]);
        glDrawElements(GL_TRIANGLES, indices.second().size(), GL_UNSIGNED_INT, &indices.second()[0]);
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_INDEX_ARRAY);
    }

    glFlush();
    SwapBuffers();
}

NAMESPACE_SPH_END
