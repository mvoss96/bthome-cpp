# Header-only library: just add bthome-cpp/src to the include path.
# mbedtls ships with ESP-IDF, so the mbedtls crypto backend works out of the box.
idf_component_register(SRCS "main.cpp"
                       INCLUDE_DIRS "." "../../../src"
                       REQUIRES bt nvs_flash mbedtls)
