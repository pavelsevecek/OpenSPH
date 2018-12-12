TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += ../lib
DEPENDPATH += . ../lib
LIBS += ../lib/liblib.a
PRE_TARGETDEPS += ../lib/liblib.a

include(../lib/inc.pro)

SOURCES += main.cpp
