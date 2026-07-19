cmake_minimum_required(VERSION 3.20)

include(env.cmake)

project(ufedmd C)

message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")

add_executable(ufedmd src/main.c)

target_compile_options(ufedmd PRIVATE -Wall -Wextra -Werror)

# Optional: debug build support
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ufedmd PRIVATE -O0 -g)
endif()

