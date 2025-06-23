# CSE3000 Research Project: Pixel Art 2.0 (Pixel Fixer)

## Project Info
This codebase is part of my [Research Project (CSE3000)](https://github.com/TU-Delft-CSE/Research-Project), the bachelor-end project of [Delft University of Technology](https://https//github.com/TU-Delft-CSE). It accompanies my bachelor thesis, aimed at creating techniques for correcting banding and pillow-shading in pixel art.

- **Name**: Rares Bites
- **Email**: bitesrares "at" gmail "dot" com

### Libraries Used
- [Dear ImGui](https://github.com/ocornut/imgui) – GUI
- [GLFW](https://github.com/glfw/glfw) – Windowing
- OpenGL – Graphics
- OpenCV – Image processing
- glm – Mathematics
- [stb](https://github.com/nothings/stb) – Single-header libraries for image/file handling
- [concave hull](https://github.com/senhorsolar/concavehull) – Open-source library to compute concave hulls

---

## How to Run

### Prerequisites
- **C++20** compatible compiler
- **CMake 3.31+**
- Installed: OpenGL, OpenCV, glm

> **Note**: This codebase is configured for ARM64 architecture. If you're using x86/x64 or another platform, you may need to adjust library dependencies or `CMakeLists.txt`.

### Steps
1. Open the project in a CMake-compatible IDE.
2. Optional: Configure CMake.
3. Build the project.
4. Run the application.

## Usage
Once the application is open, you can display an existing image from `assets/images` (where you can also manually add your own images). You can choose which algorithm you wish to run on the displayed Pixel Art, and observe the corrections made.

![demo.png](demo.png)
