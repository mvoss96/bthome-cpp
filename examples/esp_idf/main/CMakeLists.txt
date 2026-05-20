# Header-only library: just add bthome-cpp/src to the include path.
# (Alternatively, copy the whole bthome-cpp folder into your project's
#  components/ dir; it registers itself as a component via its CMakeLists.txt.)
idf_component_register(SRCS "main.cpp"
                       INCLUDE_DIRS "." "../../../src"
                       REQUIRES bt nvs_flash)
