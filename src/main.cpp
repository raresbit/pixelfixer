#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>


#include "../include/Canvas.h"

/**
 * GLFW error callback. Logs the error to the standard error stream.
 *
 * @param error Error code.
 * @param description Description of the error.
 */
static void glfw_error_callback(int error, const char *description) {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

/**
 * Custom theme for ImGui.
 */
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

/**
 * Loads a font for ImGui from a TTF file.
 *
 * @return Pointer to the loaded font, or nullptr otherwise.
 */
ImFont *loadFont() {
    ImFont *font = ImGui::GetIO().Fonts->AddFontFromFileTTF("../assets/fonts/Open_Sans/static/OpenSans-Regular.ttf",
                                                            18.0f);
    return font;
}

/**
 * @brief Retrieves a list of image files from a directory.
 *
 * @param path path to the directory.
 * @return vector of image filenames.
*/
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

/**
 * Creates an OpenGL texture from a Canvas object.
 * @param canvas the input canvas.
 * @return The ID of the created texture.
 */
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

/**
 * Main loop of the application.
 * @return exit code.
 */
int main() {
    // Set-up GLFW
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    auto glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

    GLFWwindow *window = glfwCreateWindow(720, 540, "PixelArt 2.0", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Set-up ImGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;

    applyTheme();

    if (ImFont *font = loadFont())
        io.FontDefault = font;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load images
    std::vector<std::string> imageFiles = getImageFilesFromDirectory("../assets/images");
    std::ranges::sort(imageFiles);
    std::string selectedImage = imageFiles.empty() ? "" : imageFiles[0];

    GLuint canvasTexture = 0;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Left-side menu
        ImGui::SetNextWindowSize(ImVec2(240, 0), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::Begin("Menu", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_AlwaysAutoResize);

        // Dropdown for selecting images
        if (!imageFiles.empty()) {
            ImGui::Text("Select Image");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##ImageCombo", selectedImage.c_str())) {
                for (const auto &filename: imageFiles) {
                    bool isSelected = (selectedImage == filename);
                    if (ImGui::Selectable(filename.c_str(), isSelected)) {
                        selectedImage = filename;
                    }
                }
                ImGui::EndCombo();
            }
        } else {
            ImGui::Text("No images available.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Run Algorithm");
        if (ImGui::Button("Run")) {
            // ...
        }

        ImGui::End();

        // Right-side menu with the displayed Pixel Art
        ImGui::SetNextWindowPos(ImVec2(240, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(1040, 720), ImGuiCond_Always);
        ImGui::Begin("Pixel Artwork", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoTitleBar);

        // Initialize canvas
        if (!selectedImage.empty()) {
            std::string imagePath = "../assets/images/" + selectedImage;

            static std::string lastLoadedImage;
            static Canvas canvas(1, 1);

            if (selectedImage != lastLoadedImage) {
                lastLoadedImage = selectedImage;
                if (canvas.loadFromFile(imagePath)) {
                    if (canvasTexture != 0) {
                        glDeleteTextures(1, &canvasTexture);
                    }
                    canvasTexture = createTextureFromCanvas(canvas);
                } else {
                    std::cerr << "Failed to load image: " << imagePath << std::endl;
                }
            }
            if (canvasTexture != 0) {
                constexpr float zoomFactor = 8.0f;
                const float width = zoomFactor * static_cast<float>(canvas.getWidth());
                const float height = zoomFactor * static_cast<float>(canvas.getHeight());

                ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(canvasTexture)),
                             ImVec2(width, height));
            }
        }

        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.18f, 0.18f, 0.18f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
