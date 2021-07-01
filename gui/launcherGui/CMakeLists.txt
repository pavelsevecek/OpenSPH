set(LAUNCHERGUI_SOURCES LauncherGui.h LauncherGui.cpp)
add_executable(opensph ${LAUNCHERGUI_SOURCES})

target_include_directories(opensph PRIVATE .. ../../lib)
target_link_libraries(opensph PRIVATE gui)
