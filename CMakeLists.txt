cmake_minimum_required(VERSION 3.20)

project(ufedmd C)

option(BUILD_STANDALONE "Build as standalone application" ON)

message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")

if(BUILD_STANDALONE)
    include(app.cmake)
else()
    include(library.cmake)
endif()

# Optional: debug build support
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ufedmd PRIVATE -O0 -g)
endif()
