TEMPLATE = app
CONFIG += console c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -Wall -Werror -msse4.1 -std=c++1z -pthread
QMAKE_CXX = g++


CONFIG(release, debug|release) {
  message( "Building for Release" )
}

CONFIG(debug, debug|release) {
  message( "Building for Debug" )
  DEFINES += DEBUG PROFILE
}

INCLUDEPATH += ../lib ../../external/benchmark/include
DEPENDPATH += . ../lib
LIBS += ../lib/libsph.a -lbenchmark
SOURCES += main.cpp \
    ../lib/objects/finders/benchmark/Finders.cpp
