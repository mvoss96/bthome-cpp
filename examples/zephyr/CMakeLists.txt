cmake_minimum_required(VERSION 3.20)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bthome_zephyr_example)

target_sources(app PRIVATE src/main.cpp)
# Header-only: point at bthome-cpp/src.
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
