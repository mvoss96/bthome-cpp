# Minimal ESP-IDF project. bthome.h is pulled in via the include path set in
# main/CMakeLists.txt, so no extra component wiring is needed here.
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(bthome_idf_example)
