#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>

#include "Canvas.h"

ImFont* loadFont() {
    ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF("../assets/fonts/Open_Sans/static/OpenSans-Regular.ttf", 18.0f);
    return font;
}

static void glfw_error_callback(int error, const char *description) {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

GLuint createTextureFromCanvas(const Canvas &canvas) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, canvas.getWidth(), canvas.getHeight(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, canvas.getData().data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return textureID;
}

// Apply a modern ImGui style
void applyModernStyle() {
    ImGui::StyleColorsDark();

    // Modify the default style to a more modern look
    ImGuiStyle& style = ImGui::GetStyle();
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

int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    const char *glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

    GLFWwindow *window = glfwCreateWindow(1280, 720, "PixelArt 2.0", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;

    applyModernStyle();

    // Load the modern font
    if (ImFont* font = loadFont())
        io.FontDefault = font;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Create a sample canvas
    Canvas canvas(64, 64);

    // Fill it with a sample gradient
    for (int y = 0; y < canvas.getHeight(); ++y) {
        for (int x = 0; x < canvas.getWidth(); ++x) {
            canvas.setPixel(x, y, Pixel(x * 4, y * 4, 128));
        }
    }

    const GLuint canvasTexture = createTextureFromCanvas(canvas);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Left Menu (Fixed, no overlap)
        ImGui::SetNextWindowSize(ImVec2(240, 0), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::Begin("Menu", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);
        if (ImGui::Button("Run")) {
            // Nothing yet
        }
        ImGui::End();

        // Right - Pixel Art (Fixed next to the menu)
        ImGui::SetNextWindowPos(ImVec2(240, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(1040, 720), ImGuiCond_Always);
        ImGui::Begin("Pixel Artwork", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        // Pixel Artwork Title and Modernized UI
        ImGui::Text("Pixel Art Viewer");
        ImGui::Separator();

        // Framed image and zoom interaction
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));
        ImGui::Image(canvasTexture, ImVec2(512, 512));
        ImGui::PopStyleVar();

        ImGui::Text("Use the mouse to interact with the image.");

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
