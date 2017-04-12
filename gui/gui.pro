TEMPLATE = app
CONFIG += c++14 silent
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += /usr/include/wx-3.0 ../lib/ ..
DEPENDPATH += . ../lib ../test
LIBS += `wx-config --libs --gl-libs`
LIBS += -lGL -lGLU -lGLEW
LIBS += ../lib/libsph.a

QMAKE_CXXFLAGS += -Wall -Werror -msse4.1 -std=c++14 `wx-config --libs --cxxflags --gl-libs`
QMAKE_CXX = g++


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
    Gui.cpp \
    windows/GlPane.cpp \
    windows/OrthoPane.cpp \
    windows/Window.cpp \
    Settings.cpp \
    MainLoop.cpp

HEADERS += \
    Gui.h \
    windows/OrthoPane.h \
    windows/GlPane.h \
    Common.h \
    Renderer.h \
    windows/Window.h \
    Settings.h \
    objects/Palette.h \
    objects/Color.h \
    GuiCallbacks.h \
    problems/Collision.h \
    problems/Meteoroid.h \
    objects/Camera.h \
    objects/Point.h \
    Factory.h \
    objects/Element.h \
    objects/Movie.h \
    objects/Bitmap.h \
    MainLoop.h
