TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += ../../lib
DEPENDPATH += . ../../lib
PRE_TARGETDEPS += ../../lib/liblib.a
LIBS += ../../lib/liblib.a

include(../../lib/inc.pro)

SOURCES += FragmentationReaccumulation.cpp

