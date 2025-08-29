set(DENSITY_Sources SurfaceDensity.cpp)
add_executable(surfaceDensity ${DENSITY_Sources})

target_include_directories(surfaceDensity PRIVATE ../../core)
target_link_libraries(surfaceDensity PRIVATE core)
