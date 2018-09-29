cmake_minimum_required(VERSION 2.8)
project(lux-client)
set(CMAKE_BUILD_TYPE Debug)

set(LUX_CLIENT_VERSION_MAJOR 0)
set(LUX_CLIENT_VERSION_MINOR 0)
set(LUX_CLIENT_VERSION_PATCH 0)

set(LUX_GL_VARIANT_3_3    1)
set(LUX_GL_VARIANT_ES_2_0 2)
set(LUX_GL_VARIANT        ${LUX_GL_VARIANT_ES_2_0})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wundef -fno-exceptions")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -Og -pipe \
    -fasynchronous-unwind-tables -fstack-protector-strong -D_GLIBCXX_ASSERTIONS\
    -ftrapv")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast -flto -DNDEBUG")
set(CMAKE_CXX_STANDARD 17)

if(CMAKE_BUILD_TYPE MATCHES "Release")
    message(STATUS "enabling link-time optimizations")

    set(CMAKE_AR "gcc-ar")
    set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> qcs <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_FINISH true)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
endif()

configure_file("${PROJECT_SOURCE_DIR}/config.hpp.in"
               "${PROJECT_BINARY_DIR}/include/config.hpp")

include_directories(SYSTEM "include")
include_directories("deps/lux-shared/include" "src"
                    "${PROJECT_BINARY_DIR}/include")
add_subdirectory("deps/lux-shared")
add_subdirectory("deps/lodepng")
add_subdirectory("api")

file(GLOB_RECURSE SOURCES "src/*.cpp")
if(${LUX_GL_VARIANT} EQUAL ${LUX_GL_VARIANT_3_3})
    list(APPEND SOURCES "${PROJECT_SOURCE_DIR}/glad/glad-3.3.cpp")
elseif(${LUX_GL_VARIANT} EQUAL ${LUX_GL_VARIANT_ES_2_0})
    list(APPEND SOURCES "${PROJECT_SOURCE_DIR}/glad/glad-es-2.0.cpp")
else()
    message(FATAL_ERROR "Unsupported GL variant selected")
endif()
add_executable(lux-client ${SOURCES})

find_package(Threads REQUIRED)
find_library(ENET_LIB enet)
find_library(GLFW_LIB glfw)
find_library(GL_LIB GL)
find_library(DL_LIB dl)
find_library(LUAJIT_LIB luajit)

target_link_libraries(lux-client lux)
target_link_libraries(lux-client lodepng)
target_link_libraries(lux-client Threads::Threads)
target_link_libraries(lux-client "${ENET_LIB}")
target_link_libraries(lux-client "${GLFW_LIB}")
target_link_libraries(lux-client "${GL_LIB}")
target_link_libraries(lux-client "${DL_LIB}")
target_link_libraries(lux-client "${LUAJIT_LIB}")

