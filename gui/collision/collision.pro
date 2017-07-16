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

QMAKE_CXXFLAGS += -Wall -Werror -msse4.1 -std=c++14 -pthread `wx-config --libs all --cxxflags --gl-libs`

CONFIG(release, debug|profile|assert|release) {
  message( "SPH COLLISION --- Building for Release" )
}

CONFIG(profile, debug|profile|assert|release) {
  message( "SPH COLLISION --- Building for Profile" )
  DEFINES += PROFILE
}

CONFIG(assert, debug|profile|assert|release) {
  message( "SPH COLLISION --- Building for Assert" )
  DEFINES += DEBUG PROFILE
  QMAKE_CXXFLAGS += -O2
}

CONFIG(debug, debug|profile|assert|release) {
  message( "SPH COLLISION --- Building for Debug" )
  DEFINES += DEBUG PROFILE
}

SOURCES += \
    Collision.cpp

HEADERS += \  
    Collision.h
