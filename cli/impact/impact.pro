TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

DEPENDPATH += . ../../lib
INCLUDEPATH += ../../lib
PRE_TARGETDEPS += ../../lib/liblib.a
LIBS += ../../lib/liblib.a

TARGET = opensph-impact
target.path = /usr/bin/
INSTALLS += target

include(../../lib/sharedLib.pro)

SOURCES += \
    Impact.cpp
