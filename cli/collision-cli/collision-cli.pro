TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -Wall -Wextra -Werror -msse4.1 -std=c++14 -pthread

DEPENDPATH += . ../../lib
INCLUDEPATH += ../../lib
PRE_TARGETDEPS += ../../lib/liblib.a
LIBS += ../../lib/liblib.a


CONFIG(release, debug|profile|assert|release) {
  message( "SPH CLI --- Building for Release" )
}

CONFIG(profile, debug|profile|assert|release) {
  message( "SPH CLI --- Building for Profile" )
  DEFINES += SPH_PROFILE
}

CONFIG(assert, debug|profile|assert|release) {
  message( "SPH CLI --- Building for Assert" )
  DEFINES += SPH_DEBUG SPH_PROFILE
  QMAKE_CXXFLAGS += -O2
}

CONFIG(debug, debug|profile|assert|release) {
  message( "SPH CLI --- Building for Debug" )
  DEFINES += SPH_DEBUG SPH_PROFILE
}

SOURCES += main.cpp
