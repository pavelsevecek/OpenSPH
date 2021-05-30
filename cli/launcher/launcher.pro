TEMPLATE = app
CONFIG += c++14 thread
CONFIG -= app_bundle
CONFIG -= qt

include(../../lib/sharedLib.pro)

DEPENDPATH += . ../../lib
INCLUDEPATH += ../../lib
PRE_TARGETDEPS += ../../lib/liblib.a
LIBS += ../../lib/liblib.a

TARGET = opensph-cli
target.path = $$PREFIX/bin/
INSTALLS += target

SOURCES += \
    Launcher.cpp
