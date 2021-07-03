set(INFO_SOURCES Info.cpp)
add_executable(opensph-info ${INFO_SOURCES})

target_include_directories(opensph-info PRIVATE ../../core)
target_link_libraries(opensph-info PRIVATE core)
