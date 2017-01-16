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
  DEFINES += PROFILE
}

CONFIG(assert, debug|profile|assert|release) {
  message( "SPH GUI --- Building for Assert" )
  DEFINES += DEBUG PROFILE
  QMAKE_CXXFLAGS += -O2
}

CONFIG(debug, debug|profile|assert|release) {
  message( "SPH GUI --- Building for Debug" )
  DEFINES += DEBUG PROFILE
}

SOURCES += \
    Gui.cpp \
    GlPane.cpp \
    OrthoPane.cpp \
    Window.cpp

HEADERS += \
    Gui.h \
    OrthoPane.h \
    GlPane.h \
    Common.h \
    Renderer.h \
    Window.h \
    Settings.h \
    Palette.h \
    Color.h \
    GuiCallbacks.h
