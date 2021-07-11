TEMPLATE = app
CONFIG += c++14 thread silent static
CONFIG -= app_bundle
CONFIG -= qt

DEPENDPATH += . ../../core
INCLUDEPATH += ../../core
PRE_TARGETDEPS += ../../core/libcore.a
LIBS += ../../core/libcore.a

include(../../core/sharedCore.pro)

SOURCES += \
    Batch.cpp
