TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += ../../lib ../.. /usr/include/wx-3.0
DEPENDPATH += .. ../../lib ../../gui
PRE_TARGETDEPS += ../../lib/liblib.a ../../gui/libgui.a
LIBS += ../../gui/libgui.a
LIBS += ../../lib/liblib.a # must be used after libgui
LIBS += `wx-config --libs`

include(../../lib/inc.pro)
QMAKE_CXXFLAGS += `wx-config --cxxflags`

HEADERS += \
    Player.h \
    TimeLine.h

SOURCES += \
    Player.cpp
