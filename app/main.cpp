/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    main.cpp
 * @version 0.2.0
 * @authors Anton Chernov
 * @date    2026-08-28
 * @date    @showdate "%m/%d/%Y"
 */

/********************************* Definition ********************************/

/**
 * @def VERSION_MAJOR
 * @brief Major version number of Cypher (breaking API changes).
 */
#define VERSION_MAJOR     0

/**
 * @def VERSION_MINOR
 * @brief Minor version number of Cypher (backwards-compatible additions).
 */
#define VERSION_MINOR     2

/**
 * @def VERSION_PATCH
 * @brief Patch version number of Cypher (backwards-compatible bug fixes).
 */
#define VERSION_PATCH     0

/**
 * @def VERSION_STRING
 * @brief Cypher version as a printable "MAJOR.MINOR.PATCH" string literal.
 * @details Assembled at compile time from the numeric version macros, so it
 * costs no RAM - suitable even for the most memory-constrained targets.
 */
#define VERSION_STR_(x)   #x
#define VERSION_XSTR_(x)  VERSION_STR_(x)
#define VERSION_STRING \
    VERSION_XSTR_(VERSION_MAJOR) "." \
    VERSION_XSTR_(VERSION_MINOR) "." \
    VERSION_XSTR_(VERSION_PATCH)

/**
 * @def IMGUI_DEFINE_MATH_OPERATORS
 * @brief Enables arithmetic operators for Dear ImGui ImVec2 values
 * @details Must be defined before imgui.h so the UI can calculate positions
 * and sizes using ImVec2 addition and subtraction
 */
#define IMGUI_DEFINE_MATH_OPERATORS

/******************************* Included files ******************************/
#include <stdio.h>

#include <SDL.h>
#include <SDL_opengl.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

/***************************** Private variables *****************************/

#ifdef __GNUC__  // GCC/MinGW only
const char version_info[] __attribute__((section(".version"), used)) =
    "FileDescription: Oscilloscope application\n"
    "FileVersion: 0.2.0.0\n"
    "ProductName: Oscilloscope\n"
    "ProductVersion: 0.2.0.0\n"
    "CompanyName: N/A\n"
    "LegalCopyright: Copyright (C) Anton Chernov, 2026\n"
    "OriginalFilename: run\n";

const char build_info[] __attribute__((section(".buildinfo"), used)) =
    "Build date: " __DATE__ " " __TIME__ "\n"
    "Compiler: GCC " __VERSION__ "\n";

#endif // __GNUC__
/**************************** Function prototypes ****************************/
static void drawOscilloscopeGrid(
    ImDrawList *drawList,
    const ImVec2 &position,
    const ImVec2 &size
);

/********************* Application Programming Interface *********************/

int main (void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_CORE
    );
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow(
        "Oscilloscope",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280,
        800,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (window == NULL) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (glContext == NULL) {
        fprintf(stderr, "OpenGL context creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init("#version 150");

    bool running = true;
    bool acquisitionRunning = false;
    bool demoMode = true;
    bool channelEnabled[] = {true, true};
    int timebase = 6;
    int voltsPerDivision[] = {2, 2};
    const char* timebases[] = {
        "4 ns/div", "20 ns/div", "100 ns/div", "1 us/div", "10 us/div",
        "100 us/div", "1 ms/div", "10 ms/div", "100 ms/div", "1 s/div"
    };
    const char* voltageScales[] = {
        "20 mV/div", "50 mV/div", "100 mV/div", "200 mV/div",
        "500 mV/div", "1 V/div", "2 V/div", "5 V/div"
    };

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit")) {
                    running = false;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Demo mode", NULL, &demoMode);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        ImGui::SetNextWindowPos(ImVec2(0.0f, ImGui::GetFrameHeight()));
        ImGui::SetNextWindowSize(
            ImGui::GetIO().DisplaySize - ImVec2(0.0f, ImGui::GetFrameHeight())
        );
        ImGui::Begin(
            "Oscilloscope Shell",
            NULL,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus
        );

        ImGui::BeginGroup();
        ImGui::TextUnformatted("Display");
        ImVec2 displaySize = ImGui::GetContentRegionAvail();
        displaySize.x -= 268.0f;
        displaySize.y -= ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild(
            "Waveform",
            displaySize,
            true,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
        );
        drawOscilloscopeGrid(
            ImGui::GetWindowDrawList(),
            ImGui::GetCursorScreenPos(),
            ImGui::GetContentRegionAvail()
        );
        ImGui::EndChild();
        ImGui::EndGroup();

        ImGui::SameLine();
        ImGui::BeginChild("Controls", ImVec2(260.0f, 0.0f), true);
        if (
            ImGui::Button(
                acquisitionRunning ? "Stop" : "Start", ImVec2(-1.0f, 32.0f))
        ) {
            acquisitionRunning = !acquisitionRunning;
        }
        ImGui::Checkbox("Demo mode", &demoMode);
        ImGui::Separator();
        ImGui::TextUnformatted("Horizontal");
        ImGui::Combo("Timebase", &timebase, timebases, IM_ARRAYSIZE(timebases));
        ImGui::Separator();
        for (int channel = 0; channel < 2; ++channel) {
            ImGui::PushID(channel);
            ImGui::Text("Channel %d", channel + 1);
            ImGui::Checkbox("Enabled", &channelEnabled[channel]);
            ImGui::Combo(
                "Scale",
                &voltsPerDivision[channel],
                voltageScales,
                IM_ARRAYSIZE(voltageScales)
            );
            ImGui::PopID();
        }
        ImGui::EndChild();

        ImGui::Text(
            "%s | %s | CH1 %s | CH2 %s",
            acquisitionRunning ? "Acquiring" : "Stopped",
            demoMode ? "Demo" : "No device",
            channelEnabled[0] ? "on" : "off",
            channelEnabled[1] ? "on" : "off"
        );
        ImGui::End();

        ImGui::Render();
        int displayWidth = 0;
        int displayHeight = 0;
        SDL_GL_GetDrawableSize(window, &displayWidth, &displayHeight);
        glViewport(0, 0, displayWidth, displayHeight);
        glClearColor(0.035f, 0.045f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
/***************************** Private functions *****************************/

static void drawOscilloscopeGrid(ImDrawList* drawList, const ImVec2& position,
    const ImVec2& size) {
    const ImU32 majorColor = IM_COL32(42, 75, 94, 255);
    const ImU32 minorColor = IM_COL32(25, 46, 60, 255);
    const int divisionsX = 10;
    const int divisionsY = 8;

    drawList->AddRectFilled(position, position + size, IM_COL32(8, 18, 25, 255));
    for (int index = 0; index <= divisionsX * 5; ++index) {
        const float x = position.x + size.x * index / (divisionsX * 5);
        drawList->AddLine(
            ImVec2(x, position.y),
            ImVec2(x, position.y + size.y),
            index % 5 == 0 ? majorColor : minorColor
        );
    }
    for (int index = 0; index <= divisionsY * 5; ++index) {
        const float y = position.y + size.y * index / (divisionsY * 5);
        drawList->AddLine(
            ImVec2(position.x, y),
            ImVec2(position.x + size.x, y),
            index % 5 == 0 ? majorColor : minorColor
        );
    }
}
/*****************************************************************************/
