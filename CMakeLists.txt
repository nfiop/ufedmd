cmake_minimum_required(VERSION 3.20)

include(env.cmake)

project(ufedmd C)

message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")

find_path(YAML_INCLUDE_DIR yaml.h)
find_library(YAML_LIBRARY yaml)

set(SOURCES src/main.c
            src/config/file_parser.c
            src/config/mapping.c
            src/config/section.c
            src/config/common.c)

add_executable(ufedmd ${SOURCES})

target_include_directories(ufedmd PRIVATE ${YAML_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ufedmd PRIVATE ${YAML_LIBRARY})

target_compile_options(ufedmd PRIVATE -Wall -Wextra -Werror)

# Optional: debug build support
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ufedmd PRIVATE -O0 -g)
endif()

