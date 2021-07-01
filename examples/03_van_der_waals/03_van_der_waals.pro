TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += ../../core
DEPENDPATH += . ../../core
PRE_TARGETDEPS += ../../core/libcore.a
LIBS += ../../core/libcore.a

include(../../core/sharedCore.pro)

SOURCES += VanDerWaals.cpp

