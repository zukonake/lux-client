cmake_minimum_required(VERSION 2.8)

set(SOURCES
    ${SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui_impl_opengl3.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui_impl_glfw.cpp
    PARENT_SCOPE)
        
