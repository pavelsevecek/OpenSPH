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

DEFINES += SPH_USE_EIGEN

QMAKE_CXXFLAGS += -Wall -Werror -msse4.1 -mavx -std=c++14 `wx-config --cxxflags`

linux-g++ {
    # disabling maybe-uninitialized because of Factory::getCamera, either gcc bug or some weird behavior
    QMAKE_CXXFLAGS += -Wno-maybe-uninitialized
}

CONFIG(release, debug|profile|assert|release) {
  message( "SPH GUI --- Building for Release" )
  QMAKE_CXXFLAGS += -O2
}

CONFIG(profile, debug|profile|assert|release) {
  message( "SPH GUI --- Building for Profile" )
  DEFINES += SPH_PROFILE
  QMAKE_CXXFLAGS += -O2
}

CONFIG(assert, debug|profile|assert|release) {
  message( "SPH GUI --- Building for Assert" )
  DEFINES += SPH_DEBUG
  QMAKE_CXXFLAGS += -O2
}

CONFIG(debug, debug|profile|assert|release) {
  message( "SPH GUI --- Building for Debug" )
  DEFINES += SPH_DEBUG
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
    windows/ParticleProbe.cpp

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
    windows/PaletteDialog.h
