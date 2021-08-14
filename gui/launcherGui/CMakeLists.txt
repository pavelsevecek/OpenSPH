set(LAUNCHERGUI_SOURCES LauncherGui.h LauncherGui.cpp)

if(WIN32)
	add_executable(opensph WIN32 ${LAUNCHERGUI_SOURCES})
else()
	add_executable(opensph ${LAUNCHERGUI_SOURCES})
endif()

target_include_directories(opensph PRIVATE .. ../../core)

target_link_libraries(opensph PRIVATE gui ${wxWidgets_LIBRARIES})
