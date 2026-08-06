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

# Optional host test targets: cmake -DBTHOME_BUILD_TESTS=ON ..
option(BTHOME_BUILD_TESTS "Build host correctness tests" OFF)
if(BTHOME_BUILD_TESTS)
    enable_testing()
    foreach(t IN ITEMS test_bthome test_factories test_decode)
        add_executable(${t} tests/${t}.cpp)
        target_link_libraries(${t} PRIVATE bthome)
        target_compile_options(${t} PRIVATE -fno-exceptions -fno-rtti -Wall -Wextra)
        add_test(NAME ${t} COMMAND ${t})
    endforeach()

    # test_encryption exercises the mbedtls and PSA adapters and therefore
    # needs libmbedcrypto; skipped with a notice where it is not installed.
    find_library(BTHOME_MBEDCRYPTO mbedcrypto)
    if(BTHOME_MBEDCRYPTO)
        add_executable(test_encryption tests/test_encryption.cpp)
        target_link_libraries(test_encryption PRIVATE bthome ${BTHOME_MBEDCRYPTO})
        target_compile_options(test_encryption PRIVATE -fno-exceptions -fno-rtti -Wall -Wextra)
        add_test(NAME test_encryption COMMAND test_encryption)
    else()
        message(STATUS "bthome: mbedcrypto not found - test_encryption is not built")
    endif()
endif()
