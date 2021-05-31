TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

DEPENDPATH += .. ../../lib ../../gui
PRE_TARGETDEPS += ../../lib/liblib.a ../../gui/libgui.a
LIBS += ../../gui/libgui.a
LIBS += ../../lib/liblib.a # must be used after libgui

include(../../lib/sharedLib.pro)
include(../sharedGui.pro)

INCLUDEPATH += ../../lib ../.. $$PREFIX/include/wx-3.0

TARGET = opensph
target.path = $$PREFIX/bin/
INSTALLS += target

SOURCES += \
    LauncherGui.cpp

HEADERS += \   
    LauncherGui.h

