cmake_minimum_required(VERSION 3.31)
project(PixelFixer LANGUAGES C CXX OBJC)

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
        src/PixelArtImage.cpp
        external/stb/stb.cpp
)

add_executable(PixelFixer
        ${PROJECT_SOURCES}
        ${IMGUI_SOURCES}
)

target_include_directories(PixelFixer PRIVATE
        src
        ${IMGUI_DIR}
        ${IMGUI_BACKENDS_DIR}
        external/stb
)

# OpenGL
find_package(OpenGL REQUIRED)
target_link_libraries(PixelFixer PRIVATE OpenGL::GL)

# OpenCV
find_package(OpenCV REQUIRED)
target_link_libraries(PixelFixer PRIVATE ${OpenCV_LIBS})
include_directories(${OpenCV_INCLUDE_DIRS})

# GLM
find_package(glm CONFIG REQUIRED)
target_link_libraries(PixelFixer PRIVATE glm::glm)

# GLFW
find_package(glfw3 3.3 REQUIRED)
target_link_libraries(PixelFixer PRIVATE glfw)
