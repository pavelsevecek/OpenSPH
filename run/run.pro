TEMPLATE = app
CONFIG += c++14 thread
CONFIG -= app_bundle
CONFIG -= qt
QMAKE_CXXFLAGS += -Wall -msse4.1 -std=c++14 -pthread
DEPENDPATH += . ../lib
INCLUDEPATH += ../lib
LIBS += ../lib/libsph.a

SOURCES += \
    main.cpp
