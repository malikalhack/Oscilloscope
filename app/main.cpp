/**
 * @file    main.cpp
 * @version 0.2.3
 * @authors Anton Chernov
 * @date    2026-08-28
 * @date    @showdate "%Y-%m-%d"
 * @par
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
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
#define VERSION_PATCH     3

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
#include <string>

#include <SDL.h>
#include <SDL_opengl.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#include "capture/acquisition_loop.h"
#include "usb_device.h"

using oscilloscope::capture::SAcquisitionLoop;
using oscilloscope::capture::EAcquisitionOperation;
using oscilloscope::capture::EAcquisitionState;
using oscilloscope::usb::EScanStatus;
using oscilloscope::usb::EUsbTransferStatus;
using oscilloscope::usb::SUsbConnection;
using oscilloscope::usb::SUsbConnectionResult;
using oscilloscope::usb::SUsbDeviceInfo;
using oscilloscope::usb::SUsbScanResult;

/***************************** Private variables *****************************/

#ifdef __GNUC__  // GCC/MinGW only
const char version_info[] __attribute__((section(".version"), used)) =
    "FileDescription: Oscilloscope application\n"
    "FileVersion: 0.2.3.0\n"
    "ProductName: Oscilloscope\n"
    "ProductVersion: 0.2.3.0\n"
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
static bool isUsbDevicePresent(
    const SUsbScanResult &scanResult,
    const SUsbDeviceInfo &deviceInfo
);
static std::string formatUsbConnectionStatus(
    const SUsbScanResult &scanResult,
    const SUsbConnection &connection
);
static std::string formatUsbConnectionError(
    const char *operation,
    const SUsbConnectionResult &result
);
static std::string formatAcquisitionError(
    EAcquisitionOperation operation,
    EUsbTransferStatus transferStatus
);
static void updateDemoMode(
    bool *demoMode,
    SUsbScanResult *usbScanResult,
    SUsbConnection *connection,
    SAcquisitionLoop *acquisitionLoop,
    SUsbDeviceInfo *connectedDevice,
    std::string *deviceStatus
);

/********************* Application Programming Interface *********************/

int main (void) {
    char *basePath = NULL;
    SDL_Surface *windowIcon = NULL;
    std::string iconPath;

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

    basePath = SDL_GetBasePath();
    if (basePath != NULL) {
        iconPath = std::string(basePath) + "oscilloscope.bmp";
        SDL_free(basePath);
        windowIcon = SDL_LoadBMP(iconPath.c_str());
        if (windowIcon != NULL) {
            SDL_SetWindowIcon(window, windowIcon);
            SDL_FreeSurface(windowIcon);
        }
        else {
            fprintf(stderr, "Window icon loading failed: %s\n", SDL_GetError());
        }
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
    bool channelEnabled[] = {true, true};
    int timebase = 6;
    int voltsPerDivision[] = {2, 2};
    SUsbScanResult usbScanResult =
        oscilloscope::usb::enumerateSupportedDevices();
    bool demoMode =
        (usbScanResult.status != EScanStatus::eSuccess) ||
        usbScanResult.devices.empty();
    SUsbConnection usbConnection = {NULL, NULL, 0U, false};
    SAcquisitionLoop acquisitionLoop;
    SUsbDeviceInfo connectedDevice = {0U, 0U, 0U, 0U, NULL};
    std::string deviceStatus = formatUsbConnectionStatus(
        usbScanResult,
        usbConnection
    );
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

        const EAcquisitionState acquisitionState =
            acquisitionLoop.status.state.load();
        if (
            acquisitionRunning &&
            ((acquisitionState == EAcquisitionState::eDeviceLost) ||
             (acquisitionState == EAcquisitionState::eFailed))
        ) {
            const std::string acquisitionError = formatAcquisitionError(
                acquisitionLoop.status.failedOperation.load(),
                acquisitionLoop.status.lastTransferStatus.load()
            );

            oscilloscope::capture::joinFinishedAcquisitionLoop(
                &acquisitionLoop
            );
            acquisitionRunning = false;
            if (acquisitionState == EAcquisitionState::eDeviceLost) {
                oscilloscope::usb::disconnectFromDevice(&usbConnection);
                connectedDevice = {0U, 0U, 0U, 0U, NULL};
                deviceStatus = "Device disconnected: " + acquisitionError;
            }
            else {
                deviceStatus = "Acquisition stopped: " + acquisitionError;
            }
        }

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit")) {
                    running = false;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                bool newDemoMode = demoMode;

                if (ImGui::MenuItem("Demo mode", NULL, &newDemoMode)) {
                    updateDemoMode(
                        &demoMode,
                        &usbScanResult,
                        &usbConnection,
                        &acquisitionLoop,
                        &connectedDevice,
                        &deviceStatus
                    );
                    acquisitionRunning = false;
                }
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
        ImVec2 statusPosition = ImVec2(0.0f, 0.0f);
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
        statusPosition = ImGui::GetCursorScreenPos();
        ImGui::EndGroup();

        ImGui::SameLine();
        ImGui::BeginChild("Controls", ImVec2(260.0f, 0.0f), true);
        ImGui::BeginDisabled(!(demoMode || usbConnection.isConnected));
        if (
            ImGui::Button(
                acquisitionRunning ? "Stop" : "Start", ImVec2(-1.0f, 32.0f))
        ) {
            if (acquisitionRunning) {
                if (!demoMode) {
                    oscilloscope::capture::stopAcquisitionLoop(
                        &acquisitionLoop
                    );
                }
                acquisitionRunning = false;
            }
            else {
                if (!demoMode) {
                    oscilloscope::capture::startAcquisitionLoop(
                        &acquisitionLoop,
                        usbConnection
                    );
                    deviceStatus = formatUsbConnectionStatus(
                        usbScanResult,
                        usbConnection
                    );
                }
                acquisitionRunning = true;
            }
        }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(usbConnection.isConnected);
        if (ImGui::Button("Rescan devices", ImVec2(-1.0f, 32.0f))) {
            usbScanResult = oscilloscope::usb::enumerateSupportedDevices();

            if (
                usbConnection.isConnected &&
                !isUsbDevicePresent(usbScanResult, connectedDevice)
            ) {
                oscilloscope::capture::stopAcquisitionLoop(&acquisitionLoop);
                acquisitionRunning = false;
                const SUsbConnectionResult disconnectResult =
                    oscilloscope::usb::disconnectFromDevice(&usbConnection);

                connectedDevice = {0U, 0U, 0U, 0U, NULL};
                if (disconnectResult.errorMessage.empty()) {
                    deviceStatus = formatUsbConnectionStatus(
                        usbScanResult,
                        usbConnection
                    );
                }
                else {
                    deviceStatus = formatUsbConnectionError(
                        "Disconnect",
                        disconnectResult
                    );
                }
            }
            else {
                deviceStatus = formatUsbConnectionStatus(
                    usbScanResult,
                    usbConnection
                );
            }
        }
        ImGui::EndDisabled();

        if (usbConnection.isConnected) {
            if (ImGui::Button("Disconnect", ImVec2(-1.0f, 32.0f))) {
                oscilloscope::capture::stopAcquisitionLoop(&acquisitionLoop);
                acquisitionRunning = false;
                const SUsbConnectionResult disconnectResult =
                    oscilloscope::usb::disconnectFromDevice(&usbConnection);

                connectedDevice = {0U, 0U, 0U, 0U, NULL};
                if (disconnectResult.errorMessage.empty()) {
                    deviceStatus = formatUsbConnectionStatus(
                        usbScanResult,
                        usbConnection
                    );
                }
                else {
                    deviceStatus = formatUsbConnectionError(
                        "Disconnect",
                        disconnectResult
                    );
                }
            }
        }
        else {
            const bool canConnect =
                !demoMode &&
                (usbScanResult.status == EScanStatus::eSuccess) &&
                !usbScanResult.devices.empty();

            ImGui::BeginDisabled(!canConnect);
            if (ImGui::Button("Connect", ImVec2(-1.0f, 32.0f))) {
                const SUsbConnectionResult connectResult =
                    oscilloscope::usb::connectToDevice(
                        usbScanResult.devices.front(),
                        &usbConnection
                    );

                if (connectResult.errorMessage.empty()) {
                    connectedDevice = usbScanResult.devices.front();
                    deviceStatus = formatUsbConnectionStatus(
                        usbScanResult,
                        usbConnection
                    );
                }
                else {
                    deviceStatus = formatUsbConnectionError(
                        "Connect",
                        connectResult
                    );
                }
            }
            ImGui::EndDisabled();
        }
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
        ImGui::Separator();
        bool newDemoMode = demoMode;

        if (ImGui::Checkbox("Demo mode", &newDemoMode)) {
            updateDemoMode(
                &demoMode,
                &usbScanResult,
                &usbConnection,
                &acquisitionLoop,
                &connectedDevice,
                &deviceStatus
            );
            acquisitionRunning = false;
        }
        ImGui::EndChild();

        ImGui::SetCursorScreenPos(statusPosition);
        ImGui::Text(
            "%s | %s | %s | CH1 %s | CH2 %s",
            acquisitionRunning
                ? (acquisitionLoop.status.state.load() ==
                    EAcquisitionState::eRecovering
                    ? "Recovering USB connection" : "Acquiring")
                : "Stopped",
            demoMode ? "Demo mode" : "Live mode",
            deviceStatus.c_str(),
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
    if (usbConnection.isConnected) {
        oscilloscope::capture::stopAcquisitionLoop(&acquisitionLoop);
        oscilloscope::usb::disconnectFromDevice(&usbConnection);
    }
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
/***************************** Private functions *****************************/

static bool isUsbDevicePresent(
    const SUsbScanResult &scanResult,
    const SUsbDeviceInfo &deviceInfo
) {
    bool isPresent = false;

    for (const SUsbDeviceInfo &device : scanResult.devices) {
        if (
            (device.vendorId == deviceInfo.vendorId) &&
            (device.productId == deviceInfo.productId) &&
            (device.busNumber == deviceInfo.busNumber) &&
            (device.deviceAddress == deviceInfo.deviceAddress)
        ) {
            isPresent = true;
            break;
        }
    }

    return isPresent;
}

static std::string formatUsbConnectionStatus(
    const SUsbScanResult &scanResult,
    const SUsbConnection &connection
) {
    std::string status;

    if (connection.isConnected) {
        status = "Connected";
    }
    else if (scanResult.status == EScanStatus::eSuccess) {
        if (scanResult.devices.empty()) {
            status = "Disconnected: No supported device";
        }
        else {
            status = "Disconnected: ";
            status += scanResult.devices.front().modelName;
            status += " detected";
        }
    }
    else {
        status = "USB error: ";
        status += scanResult.errorMessage;
    }

    return status;
}

static std::string formatUsbConnectionError(
    const char *operation,
    const SUsbConnectionResult &result
) {
    std::string status = operation;

    status += " error: ";
    status += result.errorMessage;
    return status;
}

static std::string formatAcquisitionError(
    const EAcquisitionOperation operation,
    const EUsbTransferStatus transferStatus
) {
    std::string status;

    switch (transferStatus) {
        case EUsbTransferStatus::eTimeout:
            status = "USB timeout";
            break;
        case EUsbTransferStatus::eNoDevice:
            status = "USB device lost";
            break;
        case EUsbTransferStatus::eShortTransfer:
            status = "Incomplete USB response";
            break;
        case EUsbTransferStatus::eError:
            status = "USB I/O error";
            break;
        case EUsbTransferStatus::eSuccess:
        default:
            status = "USB acquisition error";
            break;
    }

    switch (operation) {
        case EAcquisitionOperation::eBeginCommand:
            status += " while beginning command";
            break;
        case EAcquisitionOperation::eSpeedBeforeCommand:
        case EAcquisitionOperation::eSpeedBeforeResponse:
            status += " while checking connection speed";
            break;
        case EAcquisitionOperation::eCaptureStateCommand:
            status += " while sending capture-state command";
            break;
        case EAcquisitionOperation::eCaptureStateResponse:
            status += " while reading capture state";
            break;
        case EAcquisitionOperation::eNone:
        default:
            break;
    }

    return status;
}

static void updateDemoMode(
    bool *demoMode,
    SUsbScanResult *usbScanResult,
    SUsbConnection *connection,
    SAcquisitionLoop *acquisitionLoop,
    SUsbDeviceInfo *connectedDevice,
    std::string *deviceStatus
) {
    bool updateStatus = true;

    *demoMode = !*demoMode;

    if (*demoMode) {
        if (connection->isConnected) {
            oscilloscope::capture::stopAcquisitionLoop(acquisitionLoop);
            const SUsbConnectionResult disconnectResult =
                oscilloscope::usb::disconnectFromDevice(connection);

            *connectedDevice = {0U, 0U, 0U, 0U, NULL};
            if (!disconnectResult.errorMessage.empty()) {
                *deviceStatus = formatUsbConnectionError(
                    "Disconnect",
                    disconnectResult
                );
                updateStatus = false;
            }
        }
    }
    else {
        *usbScanResult = oscilloscope::usb::enumerateSupportedDevices();
    }

    if (updateStatus) {
        *deviceStatus = formatUsbConnectionStatus(*usbScanResult, *connection);
    }
}

static void drawOscilloscopeGrid(ImDrawList *drawList, const ImVec2 &position,
    const ImVec2 &size) {
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
