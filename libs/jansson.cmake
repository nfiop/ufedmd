cmake_minimum_required(VERSION 3.20)    

include(ExternalProject)

set(JANSSON_SOURCE_DIR
    "${CMAKE_SOURCE_DIR}/libs/jansson"
)

set(JANSSON_ROOT_DIR
    "${CMAKE_BINARY_DIR}/jansson"
)

set(JANSSON_BUILD_SOURCE_DIR
    "${JANSSON_ROOT_DIR}/src"
)

set(JANSSON_INSTALL_DIR
    "${JANSSON_ROOT_DIR}/install"
)

# Get the GNU target triplet from the compiler.
if(CMAKE_CROSSCOMPILING)
    execute_process(
        COMMAND "${CMAKE_C_COMPILER}" -dumpmachine
        OUTPUT_VARIABLE JANSSON_HOST
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE JANSSON_HOST_RESULT
    )

    if(NOT JANSSON_HOST_RESULT EQUAL 0 OR NOT JANSSON_HOST)
        message(FATAL_ERROR
            "Could not determine target triplet from "
            "${CMAKE_C_COMPILER}"
        )
    endif()

    message(STATUS "jansson host: ${JANSSON_HOST}")
endif()

ExternalProject_Add(jansson
    SOURCE_DIR
        "${JANSSON_BUILD_SOURCE_DIR}"

    CONFIGURE_COMMAND
        ${CMAKE_COMMAND} -E chdir
        "${JANSSON_BUILD_SOURCE_DIR}"
        autoreconf -i

        COMMAND
        ${CMAKE_COMMAND} -E env
            CC=${CMAKE_C_COMPILER}
            AR=${CMAKE_AR}
            RANLIB=${CMAKE_RANLIB}
            STRIP=${CMAKE_STRIP}
        ${CMAKE_COMMAND} -E chdir
        "${JANSSON_BUILD_SOURCE_DIR}"
        ./configure
            --prefix=${JANSSON_INSTALL_DIR}
            $<$<BOOL:${CMAKE_CROSSCOMPILING}>:--host=${JANSSON_HOST}>

    BUILD_COMMAND
        ${CMAKE_COMMAND} -E chdir
        "${JANSSON_BUILD_SOURCE_DIR}"
        ${CMAKE_MAKE_PROGRAM}

    INSTALL_COMMAND
        ${CMAKE_COMMAND} -E chdir
        "${JANSSON_BUILD_SOURCE_DIR}"
        ${CMAKE_MAKE_PROGRAM} install

    BUILD_IN_SOURCE
        TRUE

    BUILD_ALWAYS
        TRUE
)

set(JANSSON_LIBRARY "${JANSSON_INSTALL_DIR}/lib/libjansson.a")
