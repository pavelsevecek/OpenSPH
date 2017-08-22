TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -msse4.1 -Wall -Wextra -Werror -std=c++14 -pthread



CONFIG(release, debug|profile|assert|release) {
  message( "SPH POST --- Building for Release" )
}

CONFIG(profile, debug|profile|assert|release) {
  message( "SPH POST --- Building for Profile" )
  DEFINES += SPH_PROFILE
}

CONFIG(assert, debug|profile|assert|release) {
  message( "SPH POST --- Building for Assert" )
  DEFINES += SPH_DEBUG SPH_PROFILE
  QMAKE_CXXFLAGS += -O2
}

CONFIG(debug, debug|profile|assert|release) {
  message( "SPH POST --- Building for Debug" )
  DEFINES += SPH_DEBUG SPH_PROFILE
}


INCLUDEPATH += ../lib
DEPENDPATH += . ../lib
LIBS += ../lib/liblib.a
PRE_TARGETDEPS += ../lib/liblib.a

SOURCES += main.cpp
