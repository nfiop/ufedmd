cmake_minimum_required(VERSION 3.20)

set(SOURCES src/ufedmd-common/hashmap.c
            src/ufedmd-common/logging.c
            src/config/common.c
            src/config/file_parser.c
            src/config/objs/codec.c
            src/config/objs/pipeline.c
            src/config/objs/partition.c
            src/pipeline/scheme.c
            src/pipeline/codec.c
            src/pipeline/page_layout.c
            src/pipeline/codecs/linux-ecc-hamming.c
            src/pipeline/codecs/memset.c
            src/pipeline/codecs/xor.c)

add_library(ufedmd STATIC ${SOURCES})

target_include_directories(ufedmd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libs/jansson/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/ufedm-dev
        ${JANSSON_INSTALL_DIR}/include)

target_link_options(ufedmd PRIVATE -static)
