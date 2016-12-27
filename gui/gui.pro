TEMPLATE = app
CONFIG += c++14
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += /usr/include/wx-3.0 ../lib/ ..
DEPENDPATH += . ../lib ../test
LIBS += `wx-config --libs --gl-libs`
LIBS += -lGL -lGLU -lGLEW
LIBS += ../lib/libsph.a

QMAKE_CXXFLAGS += -Wall -msse4.1 -std=c++14 `wx-config --libs --cxxflags --gl-libs`
SOURCES += \
    Gui.cpp \
    GlPane.cpp \
    OrthoPane.cpp

HEADERS += \
    Gui.h \
    OrthoPane.h \
    GlPane.h \
    Callbacks.h \
    Common.h \
    Renderer.h \
    Window.h \
    Settings.h \
    Palette.h \
    Color.h
