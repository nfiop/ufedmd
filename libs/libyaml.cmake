cmake_minimum_required(VERSION 3.20)    

include(ExternalProject)

set(LIBYAML_SOURCE_DIR
    "${CMAKE_SOURCE_DIR}/libs/libyaml"
)

set(LIBYAML_ROOT_DIR
    "${CMAKE_BINARY_DIR}/libyaml"
)

set(LIBYAML_BUILD_SOURCE_DIR
    "${LIBYAML_ROOT_DIR}/src"
)

set(LIBYAML_INSTALL_DIR
    "${LIBYAML_ROOT_DIR}/install"
)

# Get the GNU target triplet from the compiler.
if(CMAKE_CROSSCOMPILING)
    execute_process(
        COMMAND "${CMAKE_C_COMPILER}" -dumpmachine
        OUTPUT_VARIABLE LIBYAML_HOST
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE LIBYAML_HOST_RESULT
    )

    if(NOT LIBYAML_HOST_RESULT EQUAL 0 OR NOT LIBYAML_HOST)
        message(FATAL_ERROR
            "Could not determine target triplet from "
            "${CMAKE_C_COMPILER}"
        )
    endif()

    message(STATUS "libyaml host: ${LIBYAML_HOST}")
endif()

ExternalProject_Add(libyaml
    SOURCE_DIR
        "${LIBYAML_BUILD_SOURCE_DIR}"

    CONFIGURE_COMMAND
        ${CMAKE_COMMAND} -E chdir
        "${LIBYAML_BUILD_SOURCE_DIR}"
        ./bootstrap

        COMMAND
        ${CMAKE_COMMAND} -E env
            CC=${CMAKE_C_COMPILER}
            AR=${CMAKE_AR}
            RANLIB=${CMAKE_RANLIB}
            STRIP=${CMAKE_STRIP}
        ${CMAKE_COMMAND} -E chdir
        "${LIBYAML_BUILD_SOURCE_DIR}"
        ./configure
            --prefix=${LIBYAML_INSTALL_DIR}
            $<$<BOOL:${CMAKE_CROSSCOMPILING}>:--host=${LIBYAML_HOST}>

    BUILD_COMMAND
        ${CMAKE_COMMAND} -E chdir
        "${LIBYAML_BUILD_SOURCE_DIR}"
        ${CMAKE_MAKE_PROGRAM}

    INSTALL_COMMAND
        ${CMAKE_COMMAND} -E chdir
        "${LIBYAML_BUILD_SOURCE_DIR}"
        ${CMAKE_MAKE_PROGRAM} install

    BUILD_IN_SOURCE
        TRUE

    BUILD_ALWAYS
        TRUE
)

set(LIBYAML_LIBRARY "${LIBYAML_INSTALL_DIR}/lib/libyaml.a")
