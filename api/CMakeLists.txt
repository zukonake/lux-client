cmake_minimum_required(VERSION 2.8)
project(lux-api)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})

include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/../include")
include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/../deps/lux-shared/include")
include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/../src")
file(GLOB_RECURSE SOURCES "*.cpp")

add_library(lux-api SHARED ${SOURCES})

target_link_libraries(lux-api lux)
