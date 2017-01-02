TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt
QMAKE_CXXFLAGS += -Wall -Werror -msse4.1 -std=c++14 -pthread
QMAKE_CXX = g++
QMAKE_CXXFLAGS_RELEASE -= -O2
QMAKE_CXXFLAGS_RELEASE += -Os
DEPENDPATH += . ../lib
INCLUDEPATH += ../lib
LIBS += ../lib/libsph.a


CONFIG(release, debug|release) {
  message( "Building for Release" )
}

CONFIG(debug, debug|release) {
  message( "Building for Debug" )
  DEFINES += DEBUG PROFILE
}

SOURCES += \
    main.cpp
