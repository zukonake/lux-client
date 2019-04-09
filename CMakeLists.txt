cmake_minimum_required(VERSION 2.8)
project(lux-client)
set(CMAKE_BUILD_TYPE Debug)

set(LUX_CLIENT_VERSION_MAJOR 0)
set(LUX_CLIENT_VERSION_MINOR 0)
set(LUX_CLIENT_VERSION_PATCH 0)

if(CYGWIN)
    #perhaps use a different fix for GLFW
    set(WIN32 ON)
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wundef -fno-exceptions -Wno-narrowing \
    -include ${PROJECT_BINARY_DIR}/config.hpp")
set(CMAKE_CXX_FLAGS_DEBUG "-g -Og -pipe -fasynchronous-unwind-tables \
    -fstack-protector-strong -D_GLIBCXX_ASSERTIONS -ftrapv")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -flto -DNDEBUG")
set(CMAKE_CXX_STANDARD 14)

if(CMAKE_BUILD_TYPE MATCHES "Release")
    message(STATUS "enabling link-time optimizations")

    set(CMAKE_AR "gcc-ar")
    set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> qcs <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_FINISH true)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
endif()

configure_file("${PROJECT_SOURCE_DIR}/config.hpp.in"
               "${PROJECT_BINARY_DIR}/config.hpp")

include_directories("src" "${PROJECT_BINARY_DIR}")
include_directories(SYSTEM "include" "deps/lux_shared/include" "${PROJECT_BINARY_DIR}/deps")

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_subdirectory("deps/lux_shared")
add_subdirectory("deps/lodepng")
add_subdirectory("deps/imgui")
add_subdirectory("deps/glad")
add_subdirectory("deps/GLFW")
add_executable(lux-client ${SOURCES})

find_package(Threads REQUIRED)

target_link_libraries(lux-client lux)
target_link_libraries(lux-client Threads::Threads)
target_link_libraries(lux-client glfw)

if(WIN32)
    find_library(GDI32_LIB gdi32)
    find_library(OPENGL32_LIB opengl32)
    target_link_libraries(lux-client "${GDI32_LIB}")
    target_link_libraries(lux-client "${OPENGL32_LIB}")
else()
    find_library(GL_LIB GL)
    find_library(DL_LIB dl)
    target_link_libraries(lux-client "${GL_LIB}")
    target_link_libraries(lux-client "${DL_LIB}")
endif()

