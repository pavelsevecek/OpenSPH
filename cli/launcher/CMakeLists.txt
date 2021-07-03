set(LAUNCHER_SOURCES Launcher.cpp)
add_executable(opensph-cli ${LAUNCHER_SOURCES})

target_include_directories(opensph-cli PRIVATE ../../core)
target_link_libraries(opensph-cli PRIVATE core)
