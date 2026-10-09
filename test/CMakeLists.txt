find_path(CATCH2_INCLUDE_DIR catch.hpp PATH_SUFFIXES catch2)
if (NOT CATCH2_INCLUDE_DIR)
    message(FATAL_ERROR "HDF5 tests require Catch2 v2; set CATCH2_INCLUDE_DIR to the directory containing catch.hpp")
endif()

add_executable(hdf5-tests Hdf5Main.cpp ../core/io/test/Hdf5.cpp)
target_include_directories(hdf5-tests PRIVATE ../core ${CATCH2_INCLUDE_DIR})
target_link_libraries(hdf5-tests PRIVATE core)
add_test(NAME hdf5-io COMMAND hdf5-tests "[hdf5]")
