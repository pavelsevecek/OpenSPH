TEMPLATE = app
CONFIG += c++14
CONFIG -= app_bundle
CONFIG -= qt
QMAKE_CXXFLAGS += -Wall -msse4.1 -std=c++14
DEPENDPATH += . ../src
INCLUDEPATH += ../src
LIBS += ../src/libsph.a

SOURCES += \
    main.cpp
