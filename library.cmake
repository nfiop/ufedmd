cmake_minimum_required(VERSION 3.20)

add_library(ufedmd STATIC ${BASE_SOURCES})

target_include_directories(ufedmd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libs/jansson/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/ufedm-dev
        ${JANSSON_INSTALL_DIR}/include)

target_link_options(ufedmd PRIVATE -static)
