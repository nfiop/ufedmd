cmake_minimum_required(VERSION 3.20)

include(env.cmake)

project(ufedmd C)

include (libs/libyaml.cmake)

message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")

set(SOURCES src/main.c
            src/config/file_parser.c
            src/config/mapping.c
            src/config/section.c
            src/config/common.c)

set(KERNEL_USER_SOURCES src/kernel/user/common/proxy_dev.c)

add_executable(ufedmd ${SOURCES} ${KERNEL_USER_SOURCES})

target_include_directories(ufedmd PRIVATE ${CMAKE_SOURCE_DIR}/libs/libyaml/include 
        ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/kernel/include
        ${CMAKE_SOURCE_DIR}/src/kernel/user/common)

target_link_libraries(ufedmd PRIVATE ${LIBYAML_LIBRARY})
add_dependencies(ufedmd libyaml)
target_compile_options(ufedmd PRIVATE -Wall -Wextra -Werror)
target_link_options(ufedmd PRIVATE -static)

# Optional: debug build support
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ufedmd PRIVATE -O0 -g)
endif()

