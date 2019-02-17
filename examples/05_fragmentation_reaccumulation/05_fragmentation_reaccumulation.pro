TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -msse4.1 -Wall -Werror -std=c++14 -pthread

INCLUDEPATH += ../../lib
DEPENDPATH += . ../../lib
PRE_TARGETDEPS += ../../lib/liblib.a
LIBS += ../../lib/liblib.a

CONFIG(release, debug|release) {}

CONFIG(debug, debug|release) {
  DEFINES += SPH_DEBUG SPH_PROFILE
}

SOURCES += FragmentationReaccumulation.cpp

