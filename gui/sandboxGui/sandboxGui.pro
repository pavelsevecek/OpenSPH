TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

DEPENDPATH += .. ../../core ../../gui
PRE_TARGETDEPS += ../../core/libcore.a ../../gui/libgui.a
LIBS += ../../gui/libgui.a
LIBS += ../../core/libcore.a # must be used after libgui

include(../../core/sharedCore.pro)
include(../sharedGui.pro)

INCLUDEPATH += ../../core ../.. $$PREFIX/include/wx-3.0

SOURCES += \
    SandboxGui.cpp
