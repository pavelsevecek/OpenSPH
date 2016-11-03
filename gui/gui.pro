TEMPLATE = app
CONFIG += c++14
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += /usr/include/wx-3.0 ../src/ ..
DEPENDPATH += . ../src ../test
LIBS += `wx-config --libs --gl-libs`
LIBS += -lGL -lGLU -lGLEW
LIBS += ../src/libsph.a

QMAKE_CXXFLAGS += -Wall -msse4.1 -std=c++14 `wx-config --libs --cxxflags --gl-libs`
SOURCES += gui.cpp \
    glpane.cpp

HEADERS += \
    gui.h \
    callbacks.h \
    glpane.h \
    common.h
