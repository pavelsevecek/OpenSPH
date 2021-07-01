TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += ../core
DEPENDPATH += . ../core
LIBS += ../core/libcore.a
PRE_TARGETDEPS += ../core/libcore.a

include(../core/sharedCore.pro)

SOURCES += main.cpp
