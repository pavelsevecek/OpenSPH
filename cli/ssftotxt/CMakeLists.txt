set(SSFTOTXT_SOURCES SsfToTxt.cpp)
add_executable(ssftotxt ${SSFTOTXT_SOURCES})

target_include_directories(ssftotxt PRIVATE ../../core)
target_link_libraries(ssftotxt PRIVATE core)
