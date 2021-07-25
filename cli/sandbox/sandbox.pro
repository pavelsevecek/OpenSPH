TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

DEPENDPATH += . ../../core
INCLUDEPATH += ../../core
PRE_TARGETDEPS += ../../core/libcore.a
LIBS += ../../core/libcore.a /usr/lib/libmetis.so

include(../../core/sharedCore.pro)

SOURCES += \
    Sandbox.cpp
