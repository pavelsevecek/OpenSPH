TEMPLATE = lib
CONFIG += c++14 staticlib thread silent
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += /usr/include/wx-3.0 ../lib/ ..
DEPENDPATH += ../lib ../test
PRE_TARGETDEPS += ../lib/liblib.a
LIBS += `wx-config --libs --gl-libs`
LIBS += -lGL -lGLU -lGLEW
LIBS += ../lib/liblib.a

include(../lib/inc.pro)

QMAKE_CXXFLAGS += `wx-config --cxxflags`

linux-g++ {
    # disabling maybe-uninitialized because of Factory::getCamera, either gcc bug or some weird behavior
    QMAKE_CXXFLAGS += -Wno-maybe-uninitialized
}

SOURCES += \
    Controller.cpp \
    MainLoop.cpp \
    Settings.cpp \
    Utils.cpp \
    Factory.cpp \
    objects/Movie.cpp \
    objects/Palette.cpp \
    renderers/ParticleRenderer.cpp \
    windows/GlPane.cpp \
    windows/MainWindow.cpp \
    windows/OrthoPane.cpp \
    windows/PlotView.cpp \
    renderers/RayTracer.cpp \
    renderers/MeshRenderer.cpp \
    objects/Camera.cpp \
    objects/RenderContext.cpp \
    windows/ParticleProbe.cpp \
    Config.cpp \
    windows/PaletteDialog.cpp \
    objects/Bitmap.cpp

HEADERS += \
    ArcBall.h \
    Controller.h \
    Factory.h \
    GuiCallbacks.h \
    MainLoop.h \
    Renderer.h \
    Settings.h \
    Utils.h \
    objects/Bitmap.h \
    objects/Camera.h \
    objects/Color.h \
    objects/Colorizer.h \
    objects/GraphicsContext.h \
    objects/Movie.h \
    objects/Palette.h \
    objects/Point.h \
    objects/SvgContext.h \
    renderers/IRenderer.h \
    renderers/ParticleRenderer.h \
    windows/GlPane.h \
    windows/IGraphicsPane.h \
    windows/MainWindow.h \
    windows/OrthoPane.h \
    windows/ParticleProbe.h \
    windows/PlotView.h \
    renderers/RayTracer.h \
    renderers/Brdf.h \
    Uvw.h \
    renderers/MeshRenderer.h \
    renderers/FrameBuffer.h \
    objects/RenderContext.h \
    windows/PaletteDialog.h \
    Config.h
