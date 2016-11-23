TEMPLATE = app
CONFIG += c++14 thread
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -msse4.1 -mavx -std=c++14 -pthread -pg
QMAKE_LFLAGS += -pg

INCLUDEPATH += ../lib ../../external/Catch/include
DEPENDPATH += . ../lib
LIBS += ../lib/libsph.a


#CONFIG(release, debug|profile|release) {
#  message( "Building for Release" )
#}
#CONFIG(profile, debug|profile|release) {
#  message( "Building for Profile" )
#  DEFINES += PROFILE
#}
#CONFIG(debug, debug|profile|release) {
#  message( "Building for Debug" )
#  DEFINES += DEBUG PROFILE
#}


SOURCES += \
    main.cpp \
    sod/Sod.cpp

HEADERS +=  
