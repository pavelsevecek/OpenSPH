cmake_minimum_required(VERSION 3.5)

project(OpenSPH VERSION 0.3.6 LANGUAGES CXX)

option(WITH_TBB "Enable TBB parallelization" ON)
option(WITH_EIGEN "Enable additional algorithms using Eigen" OFF)
option(WITH_CHAISCRIPT "Enable scripting tools" OFF)
option(WITH_VDB "Enable conversion to OpenVDB files" OFF)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/;${CMAKE_MODULE_PATH}")

add_definitions(-DSPH_VERSION=${CMAKE_PROJECT_VERSION})

if (WIN32)
    add_definitions(-D_ENABLE_EXTENDED_ALIGNED_STORAGE -D_CRT_SECURE_NO_WARNINGS)
endif()

find_package(wxWidgets REQUIRED COMPONENTS core base propgrid aui)
include(${wxWidgets_USE_FILE})

if (WITH_EIGEN)
    find_package(Eigen REQUIRED)
    include_directories(${EIGEN_INCLUDE_DIRS})
    add_definitions(-DSPH_USE_EIGEN)
endif()

if (WITH_CHAISCRIPT)
    add_definitions(-DSPH_USE_CHAISCRIPT)
    set(CHAISCRIPT_LIBRARIES -ldl)
endif()

if (WITH_VDB)
    find_package(OpenVDB REQUIRED)
    add_definitions(-DSPH_USE_VDB)
    include_directories(${OpenVDB_INCLUDE_DIRS})
    add_definitions(-DSPH_USE_TBB -DTBB_SUPPRESS_DEPRECATED_MESSAGES)
elseif (WITH_TBB)
    find_package(TBB REQUIRED tbb)
    add_definitions(-DSPH_USE_TBB -DTBB_SUPPRESS_DEPRECATED_MESSAGES)
endif()

if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /fp:fast")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -msse4.1")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native -mtune=native -ffast-math")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DSPH_DEBUG)
endif()

add_subdirectory(core)
add_subdirectory(gui)
add_subdirectory(gui/launcherGui)
add_subdirectory(cli/launcher)
add_subdirectory(cli/info)
