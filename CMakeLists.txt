# Header-only INTERFACE library. Works for generic CMake, Zephyr and ESP-IDF.
#
#   add_subdirectory(bthome-cpp)
#   target_link_libraries(<your_target> PRIVATE bthome)
#
# ESP-IDF: this directory also doubles as a component (see the idf branch).
cmake_minimum_required(VERSION 3.13)

if(DEFINED ESP_PLATFORM)
    # Build as an ESP-IDF component.
    idf_component_register(INCLUDE_DIRS src)
    return()
endif()

project(bthome_cpp CXX)

add_library(bthome INTERFACE)
target_include_directories(bthome INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(bthome INTERFACE cxx_std_17)
add_library(bthome::bthome ALIAS bthome)

# Optional host test target: cmake -DBTHOME_BUILD_TESTS=ON ..
option(BTHOME_BUILD_TESTS "Build host correctness test" OFF)
if(BTHOME_BUILD_TESTS)
    enable_testing()
    add_executable(test_bthome tests/test_bthome.cpp)
    target_link_libraries(test_bthome PRIVATE bthome)
    target_compile_options(test_bthome PRIVATE -fno-exceptions -fno-rtti -Wall -Wextra)
    add_test(NAME test_bthome COMMAND test_bthome)
endif()
