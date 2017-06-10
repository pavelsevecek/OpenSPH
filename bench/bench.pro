TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -Wall -Werror -msse4.1 -std=c++14 -pthread
QMAKE_CXX = g++

INCLUDEPATH += ../lib ..
DEPENDPATH += ../lib
LIBS += ../lib/liblib.a
PRE_TARGETDEPS += ../lib/liblib.a


CONFIG(release, debug|profile|assert|release) {
  message( "SPH BENCHMARK --- Building for Release" )
}

CONFIG(profile, debug|profile|assert|release) {
  message( "SPH BENCHMARK --- Building for Profile" )
  DEFINES += PROFILE
}

CONFIG(assert, debug|profile|assert|release) {
  message( "SPH BENCHMARK --- Building for Assert" )
  DEFINES += DEBUG PROFILE
  QMAKE_CXXFLAGS += -O2
}

CONFIG(debug, debug|profile|assert|release) {
  message( "SPH BENCHMARK --- Building for Debug" )
  DEFINES += DEBUG PROFILE
}

SOURCES += main.cpp \
    Session.cpp \
    examples/TestBenchmark.cpp \
    ../lib/objects/finders/benchmark/Finders.cpp

HEADERS += \
    Session.h
