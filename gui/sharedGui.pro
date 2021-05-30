QMAKE_CXXFLAGS += `wx-config --cxxflags` -Wno-deprecated-copy -Wno-array-bounds -Wno-float-conversion
LIBS += `wx-config --libs std propgrid aui`

CONFIG(use_vdb) {
    DEFINES += SPH_USE_VDB
    LIBS += -ltbb -lImath -lopenvdb
}
