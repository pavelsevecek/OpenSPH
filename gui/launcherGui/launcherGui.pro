TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += ../../lib ../.. /usr/include/wx-3.0
DEPENDPATH += .. ../../lib ../../gui
PRE_TARGETDEPS += ../../lib/liblib.a ../../gui/libgui.a
LIBS += ../../gui/libgui.a
LIBS += ../../lib/liblib.a # must be used after libgui
LIBS += `wx-config --libs` -lwx_gtk2u_propgrid-3.0 -lwx_gtk2u_aui-3.0

include(../../lib/inc.pro)
QMAKE_CXXFLAGS += `wx-config --cxxflags`

SOURCES += \
    LauncherGui.cpp

HEADERS += \   
    LauncherGui.h

