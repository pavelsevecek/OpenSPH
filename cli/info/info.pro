TEMPLATE = app
CONFIG += c++14 thread
CONFIG -= app_bundle
CONFIG -= qt

DEPENDPATH += . ../../core
INCLUDEPATH += ../../core
PRE_TARGETDEPS += ../../core/libcore.a
LIBS += ../../core/libcore.a

include(../../core/sharedCore.pro)

TARGET = opensph-info
target.path = $$PREFIX/bin/
INSTALLS += target

SOURCES += \
    Info.cpp
