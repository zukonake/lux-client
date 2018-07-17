cmake_minimum_required(VERSION 2.8)
project(lux-client)
set(CMAKE_BUILD_TYPE Debug)

set(LUX_CLIENT_VERSION_MAJOR 0)
set(LUX_CLIENT_VERSION_MINOR 0)
set(LUX_CLIENT_VERSION_PATCH 0)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})
set(THREADS_PREFER_PTHREAD_FLAG ON)

configure_file("${PROJECT_SOURCE_DIR}/config.h.in"
               "${PROJECT_BINARY_DIR}/include/config.h")

include_directories("include" "deps/lux-shared/include"
                    "src" "${PROJECT_BINARY_DIR}/include")
add_subdirectory("deps/lux-shared")
add_subdirectory("deps/lodepng")

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(lux-client ${SOURCES})

find_package(Threads REQUIRED)
find_library(ENET_LIB enet)
find_library(GLFW_LIB glfw)
find_library(GL_LIB GL)
find_library(DL_LIB dl)

target_link_libraries(lux-client lux)
target_link_libraries(lux-client lodepng)
target_link_libraries(lux-client Threads::Threads)
target_link_libraries(lux-client "${ENET_LIB}")
target_link_libraries(lux-client "${GLFW_LIB}")
target_link_libraries(lux-client "${GL_LIB}")
target_link_libraries(lux-client "${DL_LIB}")

