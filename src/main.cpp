#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <sstream>

#include "../include/Canvas.h"
#include "../include/AlgorithmModule.h"
#include "../include/BandingCorrection.h"
#include "../include/SubjectDetection.h"

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
    style.Colors[ImGuiCol_Header] = ImVec4(0.5f, 0.5f, 0.5f, 1.00f); // Gray color
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.6f, 0.6f, 0.6f, 1.00f); // Lighter gray when hovered
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.4f, 0.4f, 0.4f, 1.00f); // Slightly darker gray when active
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

ImFont *loadHeaderFont() {
    ImFont *font = ImGui::GetIO().Fonts->AddFontFromFileTTF("../assets/fonts/Open_Sans/static/OpenSans-Regular.ttf",
                                                            24.0f);
    return font;
}

bool natural_compare(const std::string& a, const std::string& b) {
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        // Skip non-digit characters
        if (!std::isdigit(a[i]) && !std::isdigit(b[j])) {
            if (a[i] != b[j]) return a[i] < b[j];
            ++i; ++j;
        } else {
            // Extract numbers from both strings
            size_t num_end_a = i, num_end_b = j;
            while (num_end_a < a.size() && std::isdigit(a[num_end_a])) ++num_end_a;
            while (num_end_b < b.size() && std::isdigit(b[num_end_b])) ++num_end_b;

            // Compare the extracted numbers
            int num_a = std::stoi(a.substr(i, num_end_a - i));
            int num_b = std::stoi(b.substr(j, num_end_b - j));

            if (num_a != num_b) return num_a < num_b;

            // Move past the number
            i = num_end_a;
            j = num_end_b;
        }
    }
    return a.size() < b.size();
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
    std::ranges::sort(files, natural_compare);

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
    glfwSetWindowSizeLimits(window, 720, 540, 720, 540);
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

std::string getExportPath() {
    std::filesystem::path base = std::filesystem::current_path();

    std::filesystem::path exportPath = base / ".." / "exports";

    if (!std::filesystem::exists(exportPath)) {
        std::filesystem::create_directory(exportPath);
    }

    std::filesystem::path filePath = exportPath / "canvas_export.png";
    return std::filesystem::absolute(filePath).string();
}

std::string getUniqueFilePath(const std::string& basePath) {
    namespace fs = std::filesystem;

    fs::path path(basePath);
    fs::path directory = path.parent_path();
    std::string stem = path.stem().string();   // e.g., "canvas_export"
    std::string extension = path.extension().string(); // e.g., ".png"

    int counter = 1;
    fs::path uniquePath = path;

    while (fs::exists(uniquePath)) {
        std::ostringstream oss;
        oss << stem << " (" << counter++ << ")" << extension;
        uniquePath = directory / oss.str();
    }

    return uniquePath.string();
}


void renderLeftMenu(bool &isDrawMode, const std::vector<std::string> &imageFiles,
                    std::string &selectedImage, GLuint &canvasTexture, Canvas &canvas,
                    std::vector<Pixel> &drawnPath,
                    const std::vector<std::unique_ptr<AlgorithmModule> > &algorithms,
                    ImFont *headerFont) {
    ImGui::SetNextWindowSize(ImVec2(240, 0), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::Begin("Menu", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);

    if (headerFont) ImGui::PushFont(headerFont);
    ImGui::Text("Set-Up");
    if (headerFont) ImGui::PopFont();
    ImGui::Spacing();

    ImGui::Text("Select Mode");
    int mode = isDrawMode ? 0 : 1;
    const char *modes[] = {"Draw", "Image"};
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::Combo("##ModeSelector", &mode, modes, IM_ARRAYSIZE(modes))) {
        if (mode == 0) {
            // Draw mode selected
            isDrawMode = true;
            canvas.fill({255, 255, 255});
            drawnPath.clear();
            for (auto &algo: algorithms) {
                if (algo) algo->reset();
            }
        } else {
            // Image mode selected
            isDrawMode = false;

            if (!imageFiles.empty()) {
                selectedImage = imageFiles[0]; // Automatically select the first image
                std::string imagePath = "../assets/images/" + selectedImage;

                if (canvas.loadFromFile(imagePath)) {
                    for (auto &algo: algorithms) {
                        if (algo) algo->reset();
                    }

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
    if (headerFont) ImGui::PushFont(headerFont);
    ImGui::Text("Algorithms");
    if (headerFont) ImGui::PopFont();

    for (const auto &algo: algorithms) {
        ImGui::PushID(algo->name().c_str());
        ImGui::Spacing();

        ImGui::Text("%s:", algo->name().c_str());

        ImGui::SameLine();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImVec2 buttonSize(40, 20);
        if (ImGui::Button("Run", buttonSize)) {
            algo->run();
        }
        ImGui::PopStyleVar();

        ImGui::Spacing();

        if (ImGui::TreeNode("Settings")) {
            algo->renderUI();
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Visual Debug"))
        {
            algo->renderDebugUI();
            ImGui::TreePop();
        }


        // Refresh texture after debug drawing, if needed
        if (canvasTexture != 0)
            glDeleteTextures(1, &canvasTexture);
        canvasTexture = createTextureFromCanvas(canvas);

        ImGui::PopID();
        ImGui::Spacing();
        ImGui::Separator();
    }

    if (headerFont) ImGui::PushFont(headerFont);
    ImGui::Text("Export");
    if (headerFont) ImGui::PopFont();

    static double saveMessageTime = -1.0f;
    static bool saveSuccess = false;

    if (ImGui::Button("Save to \"exports\"")) {
        std::string path = getExportPath();
        std::string uniquePath = getUniqueFilePath(path);
        saveSuccess = canvas.saveToFile(uniquePath);
        saveMessageTime = ImGui::GetTime();
    }

    if (saveMessageTime >= 0.0f && ImGui::GetTime() - saveMessageTime < 2.0f) {
        ImVec4 color = saveSuccess ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
        const char* message = saveSuccess ? "Canvas saved successfully!" : "Failed to save canvas.";
        ImGui::TextColored(color, "%s", message);
    }

    ImGui::End();
}

void renderCanvas(bool isDrawMode, const std::string &selectedImage, GLuint &canvasTexture, Canvas &canvas,
                  std::vector<Pixel> &drawnPath, bool &mousePressed,
                  const std::vector<std::unique_ptr<AlgorithmModule> > &algorithms) {
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
            int canvasX = static_cast<int>((mousePos.x - canvasPos.x) * static_cast<float>(canvas.getWidth()) /
                                           canvasSize.x);
            int canvasY = static_cast<int>((mousePos.y - canvasPos.y) * static_cast<float>(canvas.getHeight()) /
                                           canvasSize.y);

            if (canvasX >= 0 && canvasX < canvas.getWidth() &&
                canvasY >= 0 && canvasY < canvas.getHeight()) {
                Color black{0, 0, 0};
                canvas.setPixel({canvasX, canvasY}, black);

                if (std::ranges::none_of(drawnPath,
                                         [canvasX, canvasY](const Pixel &p) {
                                             return p.pos.x == canvasX && p.pos.y == canvasY;
                                         })) {
                    drawnPath.emplace_back(Pixel{black, {canvasX, canvasY}});
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

                    for (auto &algo: algorithms) {
                        if (algo) algo->reset();
                    }
                } else {
                    std::cerr << "Failed to load image: " << path << std::endl;
                }
            }

            if (canvasTexture != 0) {
                float zoom = 8.0f;
                ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(canvasTexture)),
                             ImVec2(static_cast<float>(canvas.getWidth()) * zoom,
                                    static_cast<float>(canvas.getHeight()) * zoom));
            }
        }
    }

    ImGui::End();
}

std::vector<std::unique_ptr<AlgorithmModule> > loadAlgorithms(Canvas &canvas) {
    std::vector<std::unique_ptr<AlgorithmModule> > algos;
    algos.emplace_back(std::make_unique<SubjectDetection>(canvas));
    algos.emplace_back(std::make_unique<BandingCorrection>(canvas));
    return algos;
}

// --- Main Application Loop ---
void runMainLoop(GLFWwindow *window) {
    const char *glsl_version = "#version 150";
    ImFont *defaultFont = nullptr;
    initImGui(window, glsl_version, defaultFont);
    ImFont *headerFont = ImGui::GetIO().Fonts->AddFontFromFileTTF("../assets/fonts/Open_Sans/static/OpenSans-Bold.ttf",
                                                                  24.0f);
    std::vector<std::string> imageFiles = getImageFilesFromDirectory("../assets/images");
    std::string selectedImage = imageFiles.empty() ? "" : imageFiles[0];

    Canvas canvas = Canvas(32, 32);
    GLuint canvasTexture = 0;
    canvas.fill({255, 255, 255});
    bool isDrawMode = false;
    bool mousePressed = false;
    std::vector<Pixel> drawnPath;
    const auto algorithms = loadAlgorithms(canvas);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        renderCanvas(isDrawMode, selectedImage, canvasTexture, canvas, drawnPath, mousePressed, algorithms);
        renderLeftMenu(isDrawMode, imageFiles, selectedImage, canvasTexture, canvas,
                       drawnPath, algorithms, headerFont);

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
