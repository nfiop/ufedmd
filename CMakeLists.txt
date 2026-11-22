cmake_minimum_required(VERSION 3.20)

project(ufedmd C)

# Include dependencies for libufedmd or just ufedmd (as an app)
include (libs/jansson.cmake)
include (libs/ufedm-dev.cmake)

option(BUILD_STANDALONE "Build as standalone application" ON)

message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")

set(BASE_SOURCES src/ufedmd-common/hashmap.c
            src/ufedmd-common/logging.c
            src/config/common.c
            src/config/file_parser.c
            src/config/objs/codec.c
            src/config/objs/pipeline.c
            src/config/objs/partition.c
            src/pipeline/scheme.c
            src/pipeline/standard_pipeline.c
            src/pipeline/codec.c
            src/pipeline/page_layout.c
            src/pipeline/codecs/linux-ecc-hamming.c
            src/pipeline/codecs/memset.c
            src/pipeline/codecs/xor.c)

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
