TEMPLATE = app
CONFIG += c++14 thread
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -msse4.1 -std=c++14 -pthread -pg
QMAKE_LFLAGS += -pg

INCLUDEPATH += ../lib ../../external/Catch/include
DEPENDPATH += . ../lib
LIBS += ../lib/libsph.a

SOURCES += \
    main.cpp \
    Sod.cpp

HEADERS +=  
