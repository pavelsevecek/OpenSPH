QMAKE_CXXFLAGS += `wx-config --cxxflags` -Wno-deprecated-copy -Wno-array-bounds -Wno-float-conversion
LIBS += `wx-config --libs` -lwx_gtk2u_propgrid-3.0 -lwx_gtk2u_aui-3.0

CONFIG(use_vdb) {
    DEFINES += SPH_USE_VDB
    INCLUDEPATH += /usr/include/openvdb
    LIBS += /usr/lib/libopenvdb.so
    QMAKE_LFLAGS += -ltbb -lIlmImf -lHalf -lblosc -lboost_iostreams -lboost_system -lboost_thread -lz -L/usr/lib/libopenvdb.so
}