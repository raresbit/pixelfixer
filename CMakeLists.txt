cmake_minimum_required(VERSION 3.31)
project(PixelArt LANGUAGES C CXX OBJC)

set(CMAKE_CXX_STANDARD 20)


# ImGUI
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
set(IMGUI_BACKENDS_DIR ${IMGUI_DIR}/backends)

set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_BACKENDS_DIR}/imgui_impl_glfw.cpp
        ${IMGUI_BACKENDS_DIR}/imgui_impl_opengl3.cpp
)

set(PROJECT_SOURCES
        src/main.cpp
        src/Canvas.cpp
)

add_executable(PixelArt
        ${PROJECT_SOURCES}
        ${IMGUI_SOURCES}
)

target_include_directories(PixelArt PRIVATE
        src
        ${IMGUI_DIR}
        ${IMGUI_BACKENDS_DIR}
)

# OpenGL
find_package(OpenGL REQUIRED)
target_link_libraries(PixelArt PRIVATE OpenGL::GL)

# GLFW
find_package(glfw3 3.3 REQUIRED)
target_link_libraries(PixelArt PRIVATE glfw)

target_link_libraries(PixelArt PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreFoundation"
        "-framework CoreGraphics"
        "-framework AppKit"
)