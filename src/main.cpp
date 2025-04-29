#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>

#include "../include/Canvas.h"
#include "../include/pixproc.h"

static void glfw_error_callback(int error, const char *description) {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void applyTheme() {
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.GrabRounding = 3.0f;
    style.FrameRounding = 3.0f;
    style.ChildRounding = 5.0f;
    style.PopupRounding = 5.0f;
    style.ScrollbarRounding = 5.0f;
    style.Colors[ImGuiCol_Header] = ImVec4(0.29f, 0.55f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.34f, 0.60f, 0.95f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.24f, 0.50f, 0.85f, 1.00f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.29f, 0.55f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.34f, 0.60f, 0.95f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.24f, 0.50f, 0.85f, 1.00f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_Text] = ImVec4(0.93f, 0.93f, 0.93f, 1.00f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.29f, 0.55f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.34f, 0.60f, 0.95f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.29f, 0.55f, 0.90f, 0.25f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.34f, 0.60f, 0.95f, 0.75f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.24f, 0.50f, 0.85f, 1.00f);
}

ImFont *loadFont() {
    ImFont *font = ImGui::GetIO().Fonts->AddFontFromFileTTF("../assets/fonts/Open_Sans/static/OpenSans-Regular.ttf",
                                                            18.0f);
    return font;
}

std::vector<std::string> getImageFilesFromDirectory(const std::string &path) {
    std::vector<std::string> files;
    for (const auto &entry: std::filesystem::directory_iterator(path)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            if (filename.ends_with(".png") || filename.ends_with(".jpg")) {
                files.push_back(filename);
            }
        }
    }
    return files;
}

GLuint createTextureFromCanvas(const Canvas &canvas) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    std::vector<unsigned char> rgbaData = canvas.getRGBAData();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, canvas.getWidth(), canvas.getHeight(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgbaData.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return textureID;
}

// HELPER METHODS FOR MAIN

GLFWwindow *initGLFW(const char *title, int width, int height, const char *glsl_version) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return nullptr;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

    GLFWwindow *window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window) return nullptr;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    return window;
}

void initImGui(GLFWwindow *window, const char *glsl_version, ImFont *&defaultFont) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();

    applyTheme();

    if (ImFont *font = loadFont())
        io.FontDefault = font;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
}

void renderLeftMenu(bool &isDrawMode, const std::vector<std::string> &imageFiles, std::string &selectedImage,
                    Canvas &canvas, GLuint &canvasTexture, std::vector<Pixel> &drawnPath) {
    ImGui::SetNextWindowSize(ImVec2(240, 0), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::Begin("Menu", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Select Mode");
    int mode = isDrawMode ? 0 : 1;
    const char *modes[] = {"Draw", "Image"};
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::Combo("##ModeSelector", &mode, modes, IM_ARRAYSIZE(modes))) {
        if (mode == 0) {
            // Draw mode selected
            isDrawMode = true;
            canvas = Canvas(32, 32);
            canvas.fill(Pixel(255, 255, 255));
            drawnPath.clear();
        } else {
            // Image mode selected
            isDrawMode = false;

            if (!imageFiles.empty()) {
                selectedImage = imageFiles[0]; // Automatically select first image
                std::string imagePath = "../assets/images/" + selectedImage;

                if (canvas.loadFromFile(imagePath)) {
                    if (canvasTexture != 0) {
                        glDeleteTextures(1, &canvasTexture);
                    }
                    canvasTexture = createTextureFromCanvas(canvas);
                } else {
                    std::cerr << "Failed to auto-load image: " << imagePath << std::endl;
                }
            }
        }
    }


    if (!isDrawMode) {
        if (!imageFiles.empty()) {
            ImGui::Text("Select Image");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##ImageCombo", selectedImage.c_str())) {
                for (const auto &filename: imageFiles) {
                    if (ImGui::Selectable(filename.c_str(), selectedImage == filename))
                        selectedImage = filename;
                }
                ImGui::EndCombo();
            }
        } else {
            ImGui::Text("No images available.");
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Run Algorithm")) {
        std::vector<Pixel> processed = pixproc::pixelPerfect(drawnPath);
        canvas.fill(Pixel(255, 255, 255));

        for (const auto &px: processed)
            canvas.setPixel(px.x, px.y, Pixel(0, 0, 0));

        if (canvasTexture != 0)
            glDeleteTextures(1, &canvasTexture);

        canvasTexture = createTextureFromCanvas(canvas);
    }

    ImGui::End();
}

void renderCanvas(bool isDrawMode, const std::string &selectedImage, Canvas &canvas, GLuint &canvasTexture,
                  std::vector<Pixel> &drawnPath, bool &mousePressed) {
    ImGui::SetNextWindowPos(ImVec2(240, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(1040, 720), ImGuiCond_Always);
    ImGui::Begin("Pixel Artwork", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    if (isDrawMode) {
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImVec2(256, 256);

        if (mousePressed) {
            int canvasX = (mousePos.x - canvasPos.x) * canvas.getWidth() / canvasSize.x;
            int canvasY = (mousePos.y - canvasPos.y) * canvas.getHeight() / canvasSize.y;

            if (canvasX >= 0 && canvasX < canvas.getWidth() &&
                canvasY >= 0 && canvasY < canvas.getHeight()) {
                Pixel black(0, 0, 0);
                canvas.setPixel(canvasX, canvasY, black);

                if (std::ranges::none_of(drawnPath,
                                         [canvasX, canvasY](const Pixel &p) { return p.x == canvasX && p.y == canvasY; })) {
                    drawnPath.emplace_back(black.r, black.g, black.b, canvasX, canvasY);
                }
            }
        }

        if (ImGui::IsMouseClicked(0)) mousePressed = true;
        if (ImGui::IsMouseReleased(0)) mousePressed = false;

        if (canvasTexture != 0) glDeleteTextures(1, &canvasTexture);
        canvasTexture = createTextureFromCanvas(canvas);
        ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(canvasTexture)), canvasSize);
    } else {
        if (!selectedImage.empty()) {
            static std::string lastLoadedImage;
            if (selectedImage != lastLoadedImage) {
                lastLoadedImage = selectedImage;
                std::string path = "../assets/images/" + selectedImage;
                if (canvas.loadFromFile(path)) {
                    if (canvasTexture != 0)
                        glDeleteTextures(1, &canvasTexture);
                    canvasTexture = createTextureFromCanvas(canvas);
                } else {
                    std::cerr << "Failed to load image: " << path << std::endl;
                }
            }

            if (canvasTexture != 0) {
                float zoom = 8.0f;
                ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(canvasTexture)),
                             ImVec2(canvas.getWidth() * zoom, canvas.getHeight() * zoom));
            }
        }
    }

    ImGui::End();
}

// --- Main Application Loop ---
void runMainLoop(GLFWwindow *window) {
    const char *glsl_version = "#version 150";
    ImFont *defaultFont = nullptr;
    initImGui(window, glsl_version, defaultFont);

    std::vector<std::string> imageFiles = getImageFilesFromDirectory("../assets/images");
    std::ranges::sort(imageFiles);
    std::string selectedImage = imageFiles.empty() ? "" : imageFiles[0];

    GLuint canvasTexture = 0;
    Canvas canvas(32, 32);
    canvas.fill(Pixel(255, 255, 255));
    bool isDrawMode = false;
    bool mousePressed = false;
    std::vector<Pixel> drawnPath;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        renderLeftMenu(isDrawMode, imageFiles, selectedImage, canvas, canvasTexture, drawnPath);
        renderCanvas(isDrawMode, selectedImage, canvas, canvasTexture, drawnPath, mousePressed);

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.18f, 0.18f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    if (canvasTexture != 0)
        glDeleteTextures(1, &canvasTexture);
}

// --- Cleanup ---
void cleanup(GLFWwindow *window) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}

// --- Main ---
int main() {
    const char *glsl_version = "#version 150";
    GLFWwindow *window = initGLFW("PixelArt 2.0", 720, 540, glsl_version);
    if (!window) return 1;

    runMainLoop(window);
    cleanup(window);

    return 0;
}
