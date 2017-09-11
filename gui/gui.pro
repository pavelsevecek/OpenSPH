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

QMAKE_CXXFLAGS += -Wall -msse4.1 -std=c++14 `wx-config --libs all --cxxflags --gl-libs`

CONFIG(release, debug|profile|assert|release) {
  message( "SPH GUI --- Building for Release" )
}

CONFIG(profile, debug|profile|assert|release) {
  message( "SPH GUI --- Building for Profile" )
  DEFINES += SPH_PROFILE
}

CONFIG(assert, debug|profile|assert|release) {
  message( "SPH GUI --- Building for Assert" )
  DEFINES += SPH_DEBUG SPH_PROFILE
  QMAKE_CXXFLAGS += -O2
}

CONFIG(debug, debug|profile|assert|release) {
  message( "SPH GUI --- Building for Debug" )
  DEFINES += SPH_DEBUG SPH_PROFILE
}

SOURCES += \
    Controller.cpp \
    MainLoop.cpp \
    Settings.cpp \
    Utils.cpp \
    objects/Factory.cpp \
    objects/Movie.cpp \
    objects/Palette.cpp \
    renderers/ParticleRenderer.cpp \
    renderers/SurfaceRenderer.cpp \
    windows/GlPane.cpp \
    windows/MainWindow.cpp \
    windows/OrthoPane.cpp \
    windows/PlotView.cpp

HEADERS += \
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
    objects/Movie.h \
    objects/Palette.h \
    objects/Point.h \
    renderers/IRenderer.h \
    renderers/ParticleRenderer.h \
    renderers/SurfaceRenderer.h \
    windows/GlPane.h \
    windows/MainWindow.h \
    windows/OrthoPane.h \
    windows/ParticleProbe.h \
    windows/PlotView.h \
    objects/GraphicsContext.h \
    objects/SvgContext.h
