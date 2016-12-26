TEMPLATE = app
CONFIG += console c++14 thread
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -msse4.1 -std=c++1z -pthread

INCLUDEPATH += ../lib ../../external/benchmark/include
DEPENDPATH += . ../lib
LIBS += ../lib/libsph.a -lbenchmark
SOURCES += main.cpp \
    ../lib/objects/finders/benchmark/Finders.cpp
