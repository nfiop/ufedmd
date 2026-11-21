cmake_minimum_required(VERSION 3.20)

project(ufedmd C)

# Include dependencies for libufedmd or just ufedmd (as an app)
include (libs/jansson.cmake)
include (libs/ufedm-dev.cmake)

option(BUILD_STANDALONE "Build as standalone application" ON)

message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")

if(BUILD_STANDALONE)
    include(app.cmake)
else()
    include(library.cmake)
endif()

add_dependencies(ufedmd jansson ufedm-dev)
target_link_libraries(ufedmd PRIVATE ${JANSSON_LIBRARY} ${UFEDM_DEV_LIBRARY})
target_compile_options(ufedmd PRIVATE -Wall -Wextra -Werror)

# Optional: debug build support
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ufedmd PRIVATE -O0 -g)
endif()
