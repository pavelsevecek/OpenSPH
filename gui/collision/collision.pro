
TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += ../../lib ../.. /usr/include/wx-3.0
DEPENDPATH += .. ../../lib ../../gui
PRE_TARGETDEPS += ../../lib/liblib.a ../../gui/libgui.a
LIBS += ../../gui/libgui.a
LIBS += ../../lib/liblib.a # must be used after libgui
LIBS += `wx-config --libs --gl-libs`


INCLUDEPATH += /usr/include/eigen3
DEFINES += SPH_USE_EIGEN

QMAKE_CXXFLAGS += -Wall -msse4.1 -mavx -std=c++14 -pthread `wx-config --cxxflags`

CONFIG(release, debug|profile|assert|release) {
  message( "SPH COLLISION --- Building for Release" )
}

CONFIG(profile, debug|profile|assert|release) {
  message( "SPH COLLISION --- Building for Profile" )
  DEFINES += SPH_PROFILE
}

CONFIG(assert, debug|profile|assert|release) {
  message( "SPH COLLISION --- Building for Assert" )
  DEFINES += SPH_DEBUG
  QMAKE_CXXFLAGS += -O2
}

CONFIG(debug, debug|profile|assert|release) {
  message( "SPH COLLISION --- Building for Debug" )
  DEFINES += SPH_DEBUG
}

SOURCES += \
    Collision.cpp

HEADERS += \  
    Collision.h

