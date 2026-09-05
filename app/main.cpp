/**
 * @file    main.cpp
 * @version 0.2.6
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

/********************************* Definitions ********************************/

/**
 * @def VERSION_MAJOR
 * @brief Major version number of Oscilloscope (breaking API changes).
 */
#define VERSION_MAJOR     0

/**
 * @def VERSION_MINOR
 * @brief Minor version number of Oscilloscope (backwards-compatible additions).
 */
#define VERSION_MINOR     2

/**
 * @def VERSION_PATCH
 * @brief Patch version number of Oscilloscope (backwards-compatible bug fixes).
 */
#define VERSION_PATCH     6

/**
 * @def VERSION_STR_
 * @brief Stringizes its argument through the preprocessor
 */
#define VERSION_STR_(x)   #x

/**
 * @def VERSION_XSTR_
 * @brief Expands its argument, then stringizes the expansion
 */
#define VERSION_XSTR_(x)  VERSION_STR_(x)

/**
 * @def VERSION_STRING
 * @brief Oscilloscope version as a printable "MAJOR.MINOR.PATCH" string literal
 * @details Assembled at compile time from the numeric version macros, so it
 * costs no RAM - suitable even for the most memory-constrained targets.
 */
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

#include "acquisition_loop.h"
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

/****************************** Module variables ******************************/

/** @brief Interval between USB presence checks outside acquisition */
static const uint32_t kUsbPresenceIntervalMs = 1000U;

/** @brief Cleared device identity used to reset connection bookkeeping */
static const SUsbDeviceInfo kEmptyDeviceInfo = { NULL, 0U, 0U, 0U, 0U };

#ifdef __GNUC__  // GCC/MinGW only
const char kVersionInfo[] __attribute__((section(".version"), used)) =
    "FileDescription: Oscilloscope application\n"
    "FileVersion: 0.2.6.0\n"
    "ProductName: Oscilloscope\n"
    "ProductVersion: 0.2.6.0\n"
    "CompanyName: N/A\n"
    "LegalCopyright: Copyright (C) Anton Chernov, 2026\n"
    "OriginalFilename: run\n";

const char kBuildInfo[] __attribute__((section(".buildinfo"), used)) =
    "Build date: " __DATE__ " " __TIME__ "\n"
    "Compiler: GCC " __VERSION__ "\n";

#endif // __GNUC__
/***************************** Private prototypes *****************************/

/**
 * @brief Draws the oscilloscope background grid into a draw list
 * @param[in] drawList ImGui draw list to render into
 * @param[in] position Top-left corner of the grid in screen space
 * @param[in] size Grid dimensions in pixels
 */
static void drawOscilloscopeGrid(
    ImDrawList *drawList,
    const ImVec2 &position,
    const ImVec2 &size
);

/**
 * @brief Checks whether a device is still present in a scan result
 * @param[in] scanResult Latest supported-device scan result
 * @param[in] deviceInfo Device identity to look for
 * @returns True when a matching device is present
 */
static bool isUsbDevicePresent(
    const SUsbScanResult &scanResult,
    const SUsbDeviceInfo &deviceInfo
);

/**
 * @brief Builds the status-line text for the current USB connection
 * @param[in] scanResult Latest supported-device scan result
 * @param[in] connection Current USB connection state
 * @returns Human-readable connection status text
 */
static std::string formatUsbConnectionStatus(
    const SUsbScanResult &scanResult,
    const SUsbConnection &connection
);

/**
 * @brief Builds a status-line message for a failed connection operation
 * @param[in] operation Name of the operation that failed
 * @param[in] result Connection result carrying the error message
 * @returns Human-readable error text
 */
static std::string formatUsbConnectionError(
    const char *operation,
    const SUsbConnectionResult &result
);

/**
 * @brief Builds a status-line message describing an acquisition fault
 * @param[in] operation Polling operation that failed
 * @param[in] transferStatus USB transfer status of the failure
 * @returns Human-readable acquisition error text
 */
static std::string formatAcquisitionError(
    EAcquisitionOperation operation,
    EUsbTransferStatus transferStatus
);

/**
 * @brief Switches between demo and live mode and updates USB state
 * @param[in] demoModeEnabled Desired demo-mode state
 * @param[out] demoMode Demo-mode flag to update
 * @param[out] usbScanResult Scan result refreshed when leaving demo mode
 * @param[in,out] connection Connection closed when entering demo mode
 * @param[in,out] acquisitionLoop Acquisition worker stopped when needed
 * @param[out] connectedDevice Connected-device identity to reset
 * @param[out] deviceStatus Status-line text to update
 */
static void updateDemoMode(
    bool demoModeEnabled,
    bool *demoMode,
    SUsbScanResult *usbScanResult,
    SUsbConnection *connection,
    SAcquisitionLoop *acquisitionLoop,
    SUsbDeviceInfo *connectedDevice,
    std::string *deviceStatus
);

/**
 * @brief Handles a terminal acquisition fault detected on the worker
 * @param[in,out] acquisitionLoop Acquisition worker to inspect and join
 * @param[in,out] connection Connection closed when the device was lost
 * @param[out] connectedDevice Connected-device identity to reset
 * @param[out] acquisitionRunning Acquisition flag cleared on a fault
 * @param[out] deviceWasDisconnected Set when the device was lost
 * @param[out] deviceStatus Status-line text to update
 */
static void handleAcquisitionFault(
    SAcquisitionLoop *acquisitionLoop,
    SUsbConnection *connection,
    SUsbDeviceInfo *connectedDevice,
    bool *acquisitionRunning,
    bool *deviceWasDisconnected,
    std::string *deviceStatus
);

/**
 * @brief Periodically checks device presence outside acquisition
 * @param[in] demoMode True while demo mode is active
 * @param[in] acquisitionRunning True while acquisition is running
 * @param[in,out] usbScanResult Scan result refreshed on each check
 * @param[in,out] connection Connection closed when the device vanished
 * @param[out] connectedDevice Connected-device identity to reset
 * @param[in,out] deviceWasDisconnected Tracks the disconnected state
 * @param[in,out] nextPresenceCheck Tick of the next scheduled check
 * @param[out] deviceStatus Status-line text to update
 */
static void pollUsbPresence(
    bool demoMode,
    bool acquisitionRunning,
    SUsbScanResult *usbScanResult,
    SUsbConnection *connection,
    SUsbDeviceInfo *connectedDevice,
    bool *deviceWasDisconnected,
    uint32_t *nextPresenceCheck,
    std::string *deviceStatus
);

/********************* Application Programming Interface *********************/

/** @fn main */
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
    bool deviceWasDisconnected = false;
    bool channelEnabled[] = {true, true};
    int timebase = 6;
    int voltsPerDivision[] = {2, 2};
    SUsbScanResult usbScanResult =
        oscilloscope::usb::enumerateSupportedDevices();
    bool demoMode =
        (usbScanResult.status != EScanStatus::eSuccess) ||
        usbScanResult.devices.empty();
    SUsbConnection usbConnection = { NULL, NULL, {}, 0U, false };
    SAcquisitionLoop acquisitionLoop;
    SUsbDeviceInfo connectedDevice = kEmptyDeviceInfo;
    std::string deviceStatus = formatUsbConnectionStatus(
        usbScanResult,
        usbConnection
    );
    uint32_t nextUsbPresenceCheck =
        SDL_GetTicks() + kUsbPresenceIntervalMs;
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

        handleAcquisitionFault(
            &acquisitionLoop,
            &usbConnection,
            &connectedDevice,
            &acquisitionRunning,
            &deviceWasDisconnected,
            &deviceStatus
        );

        pollUsbPresence(
            demoMode,
            acquisitionRunning,
            &usbScanResult,
            &usbConnection,
            &connectedDevice,
            &deviceWasDisconnected,
            &nextUsbPresenceCheck,
            &deviceStatus
        );

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit")) {
                    running = false;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Demo mode", NULL, demoMode)) {
                    updateDemoMode(
                        !demoMode,
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

        if (usbConnection.isConnected) {
            if (ImGui::Button("Disconnect", ImVec2(-1.0f, 32.0f))) {
                oscilloscope::capture::stopAcquisitionLoop(&acquisitionLoop);
                acquisitionRunning = false;
                const SUsbConnectionResult disconnectResult =
                    oscilloscope::usb::disconnectFromDevice(&usbConnection);

                connectedDevice = kEmptyDeviceInfo;
                deviceWasDisconnected = false;
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
                    if (
                        oscilloscope::usb::getConnectedDeviceInfo(
                            usbConnection,
                            &connectedDevice
                        )
                    ) {
                        deviceWasDisconnected = false;
                        deviceStatus = formatUsbConnectionStatus(
                            usbScanResult,
                            usbConnection
                        );
                    }
                    else {
                        oscilloscope::usb::disconnectFromDevice(
                            &usbConnection
                        );
                        deviceStatus =
                            "Connect error: Cannot identify USB device";
                    }
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
        if (ImGui::Checkbox("Demo mode", &demoMode)) {
            updateDemoMode(
                demoMode,
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

/** @fn isUsbDevicePresent */
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
/*----------------------------------------------------------------------------*/

/** @fn formatUsbConnectionStatus */
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
/*----------------------------------------------------------------------------*/

/** @fn formatUsbConnectionError */
static std::string formatUsbConnectionError(
    const char *operation,
    const SUsbConnectionResult &result
) {
    std::string status = operation;

    status += " error: ";
    status += result.errorMessage;
    return status;
}
/*----------------------------------------------------------------------------*/

/** @fn formatAcquisitionError */
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
        case EAcquisitionOperation::eChannelDataCommand:
            status += " while requesting channel data";
            break;
        case EAcquisitionOperation::eChannelDataResponse:
            status += " while reading channel data";
            break;
        case EAcquisitionOperation::eCaptureStartCommand:
            status += " while starting capture";
            break;
        case EAcquisitionOperation::eTriggerEnabledCommand:
            status += " while enabling trigger";
            break;
        case EAcquisitionOperation::eNone:
        default:
            break;
    }

    return status;
}
/*----------------------------------------------------------------------------*/

/** @fn handleAcquisitionFault */
static void handleAcquisitionFault(
    SAcquisitionLoop *acquisitionLoop,
    SUsbConnection *connection,
    SUsbDeviceInfo *connectedDevice,
    bool *acquisitionRunning,
    bool *deviceWasDisconnected,
    std::string *deviceStatus
) {
    const EAcquisitionState acquisitionState =
        acquisitionLoop->status.state.load();

    if (
        *acquisitionRunning &&
        ((acquisitionState == EAcquisitionState::eDeviceLost) ||
         (acquisitionState == EAcquisitionState::eFailed))
    ) {
        const std::string acquisitionError = formatAcquisitionError(
            acquisitionLoop->status.failedOperation.load(),
            acquisitionLoop->status.lastTransferStatus.load()
        );

        oscilloscope::capture::joinFinishedAcquisitionLoop(acquisitionLoop);
        *acquisitionRunning = false;
        if (acquisitionState == EAcquisitionState::eDeviceLost) {
            oscilloscope::usb::disconnectFromDevice(connection);
            *connectedDevice = kEmptyDeviceInfo;
            *deviceWasDisconnected = true;
            *deviceStatus = "Device disconnected: " + acquisitionError;
        }
        else {
            *deviceStatus = "Acquisition stopped: " + acquisitionError;
        }
    }
}
/*----------------------------------------------------------------------------*/

/** @fn pollUsbPresence */
static void pollUsbPresence(
    const bool demoMode,
    const bool acquisitionRunning,
    SUsbScanResult *usbScanResult,
    SUsbConnection *connection,
    SUsbDeviceInfo *connectedDevice,
    bool *deviceWasDisconnected,
    uint32_t *nextPresenceCheck,
    std::string *deviceStatus
) {
    const uint32_t currentTicks = SDL_GetTicks();

    if (
        !demoMode &&
        !acquisitionRunning &&
        (static_cast<int32_t>(currentTicks - *nextPresenceCheck) >= 0)
    ) {
        *usbScanResult = oscilloscope::usb::enumerateSupportedDevices();
        *nextPresenceCheck = currentTicks + kUsbPresenceIntervalMs;

        if (
            connection->isConnected &&
            (usbScanResult->status == EScanStatus::eSuccess) &&
            !isUsbDevicePresent(*usbScanResult, *connectedDevice)
        ) {
            oscilloscope::usb::disconnectFromDevice(connection);
            *connectedDevice = kEmptyDeviceInfo;
            *deviceWasDisconnected = true;
            *deviceStatus = "Device disconnected";
        }
        else if (
            !connection->isConnected &&
            (!usbScanResult->devices.empty() ||
             !*deviceWasDisconnected)
        ) {
            if (!usbScanResult->devices.empty()) {
                *deviceWasDisconnected = false;
            }
            *deviceStatus = formatUsbConnectionStatus(
                *usbScanResult,
                *connection
            );
        }
    }
}
/*----------------------------------------------------------------------------*/

/** @fn updateDemoMode */
static void updateDemoMode(
    const bool demoModeEnabled,
    bool *demoMode,
    SUsbScanResult *usbScanResult,
    SUsbConnection *connection,
    SAcquisitionLoop *acquisitionLoop,
    SUsbDeviceInfo *connectedDevice,
    std::string *deviceStatus
) {
    bool updateStatus = true;

    *demoMode = demoModeEnabled;

    if (*demoMode) {
        if (connection->isConnected) {
            oscilloscope::capture::stopAcquisitionLoop(acquisitionLoop);
            const SUsbConnectionResult disconnectResult =
                oscilloscope::usb::disconnectFromDevice(connection);

            *connectedDevice = kEmptyDeviceInfo;
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
/*----------------------------------------------------------------------------*/

/** @fn drawOscilloscopeGrid */
static void drawOscilloscopeGrid(
    ImDrawList *drawList,
    const ImVec2 &position,
    const ImVec2 &size
) {
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
/******************************************************************************/
