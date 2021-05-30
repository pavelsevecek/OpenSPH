TEMPLATE = app
CONFIG += c++14 thread
CONFIG -= app_bundle
CONFIG -= qt

DEPENDPATH += . ../../lib
INCLUDEPATH += ../../lib
PRE_TARGETDEPS += ../../lib/liblib.a
LIBS += ../../lib/liblib.a

TARGET = opensph-info
target.path = $$PREFIX/bin/
INSTALLS += target

include(../../lib/sharedLib.pro)

SOURCES += \
    Info.cpp
