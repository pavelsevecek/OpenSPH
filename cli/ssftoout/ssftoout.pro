TEMPLATE = app
CONFIG += c++14 thread
CONFIG -= app_bundle
CONFIG -= qt


DEPENDPATH += . ../../lib
INCLUDEPATH += ../../lib
PRE_TARGETDEPS += ../../lib/liblib.a

LIBS += ../../lib/liblib.a

include(../../lib/inc.pro)

SOURCES += \
    SsfToOut.cpp
