cmake_minimum_required(VERSION 3.20)

include(env.cmake)

project(ufedmd C)

include(ExternalProject)

set(LIBYAML_SOURCE_DIR "${CMAKE_SOURCE_DIR}/libs/libyaml")
set(LIBYAML_BINARY_DIR "${CMAKE_BINARY_DIR}/libyaml")
set(LIBYAML_INSTALL_DIR "${CMAKE_BINARY_DIR}/libyaml-install")

ExternalProject_Add(libyaml
    SOURCE_DIR "${LIBYAML_SOURCE_DIR}"
    BINARY_DIR "${LIBYAML_BINARY_DIR}"

    CONFIGURE_COMMAND
        ${CMAKE_COMMAND} -E chdir "${LIBYAML_SOURCE_DIR}"
        ./bootstrap
    	COMMAND
	${CMAKE_COMMAND} -E chdir "${LIBYAML_SOURCE_DIR}"
	./configure --prefix=${LIBYAML_INSTALL_DIR}

    BUILD_COMMAND
        ${CMAKE_COMMAND} -E chdir "${LIBYAML_SOURCE_DIR}"
    	${CMAKE_MAKE_PROGRAM}

    INSTALL_COMMAND
        ${CMAKE_COMMAND} -E chdir "${LIBYAML_SOURCE_DIR}"
        ${CMAKE_MAKE_PROGRAM} install

    BUILD_IN_SOURCE FALSE
)

set(LIBYAML_LIBRARY "${LIBYAML_INSTALL_DIR}/lib/libyaml.a")

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

