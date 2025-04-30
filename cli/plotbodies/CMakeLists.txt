set(PLOTBODIES_Sources PlotBodies.cpp)
add_executable(plotbodies ${PLOTBODIES_Sources})

target_include_directories(plotbodies PRIVATE ../../core)
target_link_libraries(plotbodies PRIVATE core)
