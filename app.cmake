cmake_minimum_required(VERSION 3.20)

set(APP_SOURCES app/main.c
            app/params.c
            app/queues.c)

add_executable(ufedmd ${BASE_SOURCES} ${APP_SOURCES})

target_include_directories(ufedmd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libs/jansson/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/app
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/ufedm-dev
        ${JANSSON_INSTALL_DIR}/include)
