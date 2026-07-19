cmake_minimum_required(VERSION 3.20)

# --------------------------------------------------
# Buildroot vs Native (host) mode selection
# --------------------------------------------------
set(BUILDROOT_DIR "" CACHE PATH "Buildroot root directory")
set(TOOLCHAIN_PREFIX "" CACHE PATH "Toolchain architecture prefix")

set(KERNEL_MODULE_SRC ${CMAKE_SOURCE_DIR}/kernel)

if(BUILDROOT_DIR STREQUAL "")
    set(NATIVE_MODE ON)
else()
    set(NATIVE_MODE OFF)
endif()

message(STATUS "Native mode: ${NATIVE_MODE}")

# --------------------------------------------------
# TOOLCHAIN SELECTION (MUST HAPPEN EARLY)
# --------------------------------------------------
if(NATIVE_MODE)

    message(STATUS "Using native toolchain (gcc/g++)")

    set(CMAKE_C_COMPILER gcc CACHE STRING "" FORCE)
    set(CMAKE_CXX_COMPILER g++ CACHE STRING "" FORCE)

    execute_process(
        COMMAND uname -m
        OUTPUT_VARIABLE TOOLCHAIN_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

else()

    message(STATUS "Using Buildroot toolchain")

    set(TOOLCHAIN_FILE
        "${BUILDROOT_DIR}/output/host/share/buildroot/toolchainfile.cmake"
    )

    if(NOT EXISTS "${TOOLCHAIN_FILE}")
        message(FATAL_ERROR "Buildroot toolchain file not found: ${TOOLCHAIN_FILE}")
    endif()

    set(CMAKE_TOOLCHAIN_FILE "${TOOLCHAIN_FILE}" CACHE FILEPATH "" FORCE)

    file(GLOB TOOLCHAINS "${BUILDROOT_DIR}/output/host/usr/bin/*-gcc")
    list(GET TOOLCHAINS 0 GCC)

    get_filename_component(GCC_NAME "${GCC}" NAME)
    string(REGEX REPLACE "-gcc$" "" TOOLCHAIN_PREFIX "${GCC_NAME}")

endif()

