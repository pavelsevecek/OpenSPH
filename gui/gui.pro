TEMPLATE = lib
CONFIG += c++14 staticlib thread silent
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += /usr/include/wx-3.0 ../lib/ ..
DEPENDPATH += ../lib ../test
PRE_TARGETDEPS += ../lib/liblib.a
LIBS += `wx-config --libs`
LIBS += ../lib/liblib.a

include(../lib/inc.pro)

QMAKE_CXXFLAGS += `wx-config --cxxflags`

linux-g++ {
    # disabling maybe-uninitialized because of Factory::getCamera, either gcc bug or some weird behavior
    QMAKE_CXXFLAGS += -Wno-maybe-uninitialized
}

SOURCES += \
    Config.cpp \
    Controller.cpp \
    Factory.cpp \
    MainLoop.cpp \
    Settings.cpp \
    Utils.cpp \
    objects/Bitmap.cpp \
    objects/Camera.cpp \
    objects/Movie.cpp \
    objects/Palette.cpp \
    objects/RenderContext.cpp \
    renderers/MeshRenderer.cpp \
    renderers/ParticleRenderer.cpp \
    renderers/RayTracer.cpp \
    renderers/Spectrum.cpp \
    windows/GlPane.cpp \
    windows/MainWindow.cpp \
    windows/OrthoPane.cpp \
    windows/PaletteDialog.cpp \
    windows/ParticleProbe.cpp \
    windows/PlotView.cpp

HEADERS += \
    ArcBall.h \
    Config.h \
    Controller.h \
    Factory.h \
    GuiCallbacks.h \
    MainLoop.h \
    Renderer.h \
    Settings.h \
    Utils.h \
    Uvw.h \
    objects/Bitmap.h \
    objects/Camera.h \
    objects/Color.h \
    objects/Colorizer.h \
    objects/GraphicsContext.h \
    objects/Movie.h \
    objects/Palette.h \
    objects/Point.h \
    objects/RenderContext.h \
    objects/SvgContext.h \
    renderers/Brdf.h \
    renderers/FrameBuffer.h \
    renderers/IRenderer.h \
    renderers/MeshRenderer.h \
    renderers/ParticleRenderer.h \
    renderers/RayTracer.h \
    renderers/Spectrum.h \
    windows/GlPane.h \
    windows/IGraphicsPane.h \
    windows/MainWindow.h \
    windows/OrthoPane.h \
    windows/PaletteDialog.h \
    windows/ParticleProbe.h \
    windows/PlotView.h
