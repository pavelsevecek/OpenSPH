QMAKE_CXXFLAGS += `wx-config --cxxflags`
LIBS += `wx-config --libs` -lwx_gtk2u_propgrid-3.0 -lwx_gtk2u_aui-3.0

CONFIG(use_vdb) {
    DEFINES += SPH_USE_VDB
    INCLUDEPATH += /home/pavel/projects/astro/sph/external/openvdb
    LIBS += /home/pavel/projects/astro/sph/external/openvdb/openvdb/libopenvdb.a
    QMAKE_LFLAGS += -ltbb -lIlmImf -lHalf -lblosc -lboost_iostreams -lboost_system -lboost_thread -lz -L/home/pavel/projects/astro/sph/external/openvdb/openvdb/libopenvdb.a
}
