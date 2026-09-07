/**
 * @file    main.cpp
 * @version 0.2.12
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
#define VERSION_PATCH     12

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
#include <vector>

#include <SDL.h>
#include <SDL_opengl.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#include "acquisition_loop.h"
#include "usb_device.h"
#include "instrument_scaling_profile.h"
#include "waveform_scaling.h"
#include "waveform_trigger.h"

using oscilloscope::capture::SAcquisitionLoop;
using oscilloscope::capture::SWaveformSamples;
using oscilloscope::capture::EAcquisitionOperation;
using oscilloscope::capture::EAcquisitionState;
using oscilloscope::core::EInstrumentModel;
using oscilloscope::core::ETriggerSlope;
using oscilloscope::core::findEdgeTrigger;
using oscilloscope::core::findInstrtScalingProfile;
using oscilloscope::core::sampleIndexToSeconds;
using oscilloscope::core::sampleToVolts;
using oscilloscope::core::SInstrumentScalingProfile;
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
static const SUsbDeviceInfo kEmptyDeviceInfo =
    { NULL, EInstrumentModel::eUnknown, 0U, 0U, 0U, 0U };

/** @brief Fraction of the captured record shown across the display, leaving
 *         the rest as pre/post-trigger reserve to fill under the marker */
static const float kDisplayWindowFraction = 0.8f;

/** @brief Trigger acquisition modes offered in the control panel */
enum class ETriggerMode {
    eAuto = 0, /**< Free-running sweep, refreshes without a trigger */
    eNormal,   /**< Refreshes only when the trigger condition is met */
    eSingle    /**< Captures one triggered frame, then holds it */
};

/** @brief Trigger source selectable in the control panel */
enum class ETriggerSource {
    eChannelOne = 0,
    eChannelTwo,
    eAlternate,
    eExternal,
    eExternalTenth
};

/** @brief One trigger-mode option pairing its value with its label */
struct STriggerModeOption {
    ETriggerMode mode; /**< Enumerated mode value */
    const char *label; /**< Display text */
};

/** @brief One trigger-slope option pairing its value with its label */
struct STriggerSlopeOption {
    ETriggerSlope slope; /**< Enumerated slope value */
    const char *label;   /**< Display text */
};

/** @brief One trigger-source option: value, label, and signal channel */
struct STriggerSourceOption {
    ETriggerSource source; /**< Enumerated source value */
    const char *label;     /**< Display text */
    int signalChannel;     /**< 0=CH1, 1=CH2, -1 = no in-software signal */
};

static const STriggerModeOption kTriggerModeOptions[] = {
    { ETriggerMode::eAuto,   "Auto"   },
    { ETriggerMode::eNormal, "Normal" },
    { ETriggerMode::eSingle, "Single" }
};

static const STriggerSlopeOption kTriggerSlopeOptions[] = {
    { ETriggerSlope::eRising,  "Rising"  },
    { ETriggerSlope::eFalling, "Falling" }
};

static const STriggerSourceOption kTriggerSourceOptions[] = {
    { ETriggerSource::eChannelOne,    "CH1",     0 },
    { ETriggerSource::eChannelTwo,    "CH2",     1 },
    { ETriggerSource::eAlternate,     "ALT",     0 },
    { ETriggerSource::eExternal,      "EXT",    -1 },
    { ETriggerSource::eExternalTenth, "EXT/10", -1 }
};

static const size_t kTriggerModeOptionCount =
    sizeof(kTriggerModeOptions) / sizeof(kTriggerModeOptions[0]);
static const size_t kTriggerSlopeOptionCount =
    sizeof(kTriggerSlopeOptions) / sizeof(kTriggerSlopeOptions[0]);
static const size_t kTriggerSourceOptionCount =
    sizeof(kTriggerSourceOptions) / sizeof(kTriggerSourceOptions[0]);

#ifdef __GNUC__  // GCC/MinGW only
const char kVersionInfo[] __attribute__((section(".version"), used)) =
    "FileDescription: Oscilloscope application\n"
    "FileVersion: 0.2.12.0\n"
    "ProductName: Oscilloscope\n"
    "ProductVersion: 0.2.12.0\n"
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
 * @brief Draws one channel's waveform as a polyline into a draw list
 * @param[in] drawList ImGui draw list to render into
 * @param[in] position Top-left corner of the plot area in screen space
 * @param[in] size Plot-area dimensions in pixels
 * @param[in] samples Raw ADC bytes to plot, one per horizontal step
 * @param[in] sampleCount Total samples available in @p samples
 * @param[in] windowStart First record sample mapped to the left edge
 * @param[in] windowLength Number of samples spread across the plot width
 * @param[in] profile Scaling profile giving the ADC-to-division mapping
 * @param[in] zeroReference Raw ADC level treated as zero volts for this channel
 * @param[in] color Polyline color
 */
static void drawChannelWaveform(
    ImDrawList *drawList,
    const ImVec2 &position,
    const ImVec2 &size,
    const uint8_t *samples,
    size_t sampleCount,
    size_t windowStart,
    size_t windowLength,
    const SInstrumentScalingProfile *profile,
    double zeroReference,
    ImU32 color
);

/**
 * @brief Computes the on-screen window length in samples
 * @param[in] sampleCount Total samples in the captured record
 * @returns Window length shown across the display, leaving the remainder as
 *          pre/post-trigger reserve so the trace fills under the marker
 */
static size_t computeDisplayWindow(size_t sampleCount);

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

/**
 * @brief Resolves the display-scaling profile for the active instrument
 * @param[in] connectedDevice Currently connected device identity, if any
 * @param[in] usbScanResult Latest supported-device scan result
 * @returns Matching profile; falls back to the DSO-2250 profile so the UI
 *          always has scale steps to display, even before a connection
 */
static const SInstrumentScalingProfile* resolveActiveScalingProfile(
    const SUsbDeviceInfo &connectedDevice,
    const SUsbScanResult &usbScanResult
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
    int voltsPerDivision[] = {7, 7};
    float channelZeroReference[] = {128.0f, 128.0f};
    float channelBaselineMean[] = {128.0f, 128.0f};
    ETriggerSource triggerSource = ETriggerSource::eChannelOne;
    ETriggerSlope triggerSlope = ETriggerSlope::eRising;
    int triggerLevel = 128;
    float triggerPosition = 0.5f;
    ETriggerMode triggerMode = ETriggerMode::eAuto;
    bool singleArmed = true;
    bool heldValid = false;
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
    SWaveformSamples latestWaveform{};
    SWaveformSamples heldWaveform{};
    uint32_t latestTriggerPoint = 0U;
    uint32_t heldTriggerIndex = 0U;
    bool hasWaveform = false;

    /* Timebase/voltage-scale labels and values come from the active
     * instrument's scaling profile, resolved once per frame below. */

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

        if (
            oscilloscope::capture::getLatestWaveform(
                &acquisitionLoop, &latestWaveform, &latestTriggerPoint
            )
        ) {
            hasWaveform = true;
            /* Track per-channel mean baseline for the Set zero action. */
            if (latestWaveform.sampleCount != 0U) {
                unsigned long sumOne = 0UL;
                unsigned long sumTwo = 0UL;
                size_t sampleIndex = 0U;

                for (
                    sampleIndex = 0U;
                    sampleIndex < latestWaveform.sampleCount;
                    ++sampleIndex
                ) {
                    sumOne += latestWaveform.channelOne[sampleIndex];
                    sumTwo += latestWaveform.channelTwo[sampleIndex];
                }
                channelBaselineMean[0] = static_cast<float>(
                    static_cast<double>(sumOne) / latestWaveform.sampleCount
                );
                channelBaselineMean[1] = static_cast<float>(
                    static_cast<double>(sumTwo) / latestWaveform.sampleCount
                );
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

        const SInstrumentScalingProfile *scalingProfile =
            resolveActiveScalingProfile(connectedDevice, usbScanResult);
        const int triggerSignalChannel =
            kTriggerSourceOptions[static_cast<size_t>(triggerSource)]
                .signalChannel;
        const bool triggerHasSignal = (triggerSignalChannel >= 0);
        bool triggerFound = false;
        size_t triggerIndex = 0U;
        size_t triggerStartIndex = 0U;

        if (hasWaveform && (latestWaveform.sampleCount != 0U) &&
            triggerHasSignal) {
            const uint8_t *triggerSamples =
                (triggerSignalChannel == 1)
                    ? latestWaveform.channelTwo.data()
                    : latestWaveform.channelOne.data();
            const size_t triggerWindowSamples =
                computeDisplayWindow(latestWaveform.sampleCount);

            /* Arm the trigger past the window's pre-trigger span so the
             * aligned window keeps enough history to fill left of T. */
            triggerStartIndex = static_cast<size_t>(
                triggerPosition *
                static_cast<float>(triggerWindowSamples - 1U)
            );
            triggerFound = findEdgeTrigger(
                triggerSamples,
                latestWaveform.sampleCount,
                static_cast<uint8_t>(triggerLevel),
                triggerSlope,
                triggerStartIndex,
                &triggerIndex
            );
        }

        /* Normal and Single both hold the last triggered frame. Single
         * captures once per arm; Normal refreshes on every new trigger. */
        bool captureNow = false;
        if (triggerMode == ETriggerMode::eNormal) {
            captureNow = triggerFound;
        }
        else if (triggerMode == ETriggerMode::eSingle) {
            captureNow = singleArmed && triggerFound;
        }

        if (captureNow && hasWaveform && (latestWaveform.sampleCount != 0U)) {
            heldWaveform = latestWaveform;
            heldTriggerIndex = static_cast<uint32_t>(triggerIndex);
            heldValid = true;
            if (triggerMode == ETriggerMode::eSingle) {
                singleArmed = false;
            }
        }
        if (triggerMode == ETriggerMode::eAuto) {
            heldValid = false;
            singleArmed = true;
        }
        else if (triggerMode == ETriggerMode::eNormal) {
            singleArmed = true;
        }

        const SWaveformSamples *shownWaveform = &latestWaveform;
        size_t shownTriggerIndex = triggerIndex;
        bool shownTriggerFound = triggerFound;
        bool shownHasWaveform =
            hasWaveform && (latestWaveform.sampleCount != 0U);
        bool drawTraces = false;

        if (triggerMode == ETriggerMode::eAuto) {
            drawTraces = shownHasWaveform;
        }
        else if (heldValid) {
            shownWaveform = &heldWaveform;
            shownTriggerIndex = static_cast<size_t>(heldTriggerIndex);
            shownTriggerFound = true;
            shownHasWaveform = (heldWaveform.sampleCount != 0U);
            drawTraces = shownHasWaveform;
        }

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
        ImDrawList *waveformDrawList = ImGui::GetWindowDrawList();
        const ImVec2 waveformOrigin = ImGui::GetCursorScreenPos();
        const ImVec2 waveformSize = ImGui::GetContentRegionAvail();
        const float triggerReferenceX =
            waveformOrigin.x + waveformSize.x * triggerPosition;
        const size_t displayWindowSamples =
            computeDisplayWindow(shownWaveform->sampleCount);
        size_t displayWindowStart = 0U;

        if (shownTriggerFound && (displayWindowSamples > 1U)) {
            const float preTriggerSamples = triggerPosition *
                static_cast<float>(displayWindowSamples - 1U);
            const float windowStartFloat =
                static_cast<float>(shownTriggerIndex) - preTriggerSamples;
            size_t maxWindowStart = 0U;

            if (shownWaveform->sampleCount > displayWindowSamples) {
                maxWindowStart =
                    shownWaveform->sampleCount - displayWindowSamples;
            }
            if (windowStartFloat > 0.0f) {
                displayWindowStart =
                    static_cast<size_t>(windowStartFloat + 0.5f);
                if (displayWindowStart > maxWindowStart) {
                    displayWindowStart = maxWindowStart;
                }
            }
        }

        drawOscilloscopeGrid(waveformDrawList, waveformOrigin, waveformSize);
        if (shownHasWaveform) {
            if (drawTraces && channelEnabled[0]) {
                drawChannelWaveform(
                    waveformDrawList, waveformOrigin, waveformSize,
                    shownWaveform->channelOne.data(),
                    shownWaveform->sampleCount,
                    displayWindowStart, displayWindowSamples,
                    scalingProfile,
                    static_cast<double>(channelZeroReference[0]),
                    IM_COL32(255, 214, 0, 255)
                );
            }
            if (drawTraces && channelEnabled[1]) {
                drawChannelWaveform(
                    waveformDrawList, waveformOrigin, waveformSize,
                    shownWaveform->channelTwo.data(),
                    shownWaveform->sampleCount,
                    displayWindowStart, displayWindowSamples,
                    scalingProfile,
                    static_cast<double>(channelZeroReference[1]),
                    IM_COL32(64, 200, 255, 255)
                );
            }
            if ((scalingProfile != NULL) && triggerHasSignal) {
                const float pixelsPerDivision = waveformSize.y /
                    static_cast<float>(scalingProfile->verticalDivisions);
                const float levelDivisions =
                    (static_cast<float>(triggerLevel) -
                        channelZeroReference[triggerSignalChannel]) /
                    static_cast<float>(scalingProfile->adcCountsPerDivision);
                const float levelY = waveformOrigin.y +
                    waveformSize.y * 0.5f - levelDivisions * pixelsPerDivision;

                waveformDrawList->AddLine(
                    ImVec2(waveformOrigin.x, levelY),
                    ImVec2(waveformOrigin.x + waveformSize.x, levelY),
                    IM_COL32(255, 96, 96, 150), 1.0f
                );
                /* Vertical reference and "T" marker sit at the fixed
                 * trigger position; the trace is aligned under it. */
                waveformDrawList->AddLine(
                    ImVec2(triggerReferenceX, waveformOrigin.y),
                    ImVec2(
                        triggerReferenceX, waveformOrigin.y + waveformSize.y),
                    IM_COL32(255, 96, 96, 200), 1.0f
                );
                waveformDrawList->AddTriangleFilled(
                    ImVec2(triggerReferenceX - 5.0f, waveformOrigin.y),
                    ImVec2(triggerReferenceX + 5.0f, waveformOrigin.y),
                    ImVec2(triggerReferenceX, waveformOrigin.y + 8.0f),
                    IM_COL32(255, 96, 96, 220)
                );
                waveformDrawList->AddText(
                    ImVec2(triggerReferenceX + 6.0f, waveformOrigin.y + 1.0f),
                    IM_COL32(255, 96, 96, 255),
                    "T"
                );
            }
        }
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

        std::vector<const char*> timebaseLabels;
        std::vector<const char*> voltageScaleLabels;

        for (size_t i = 0U; i < scalingProfile->timebaseStepCount; ++i) {
            timebaseLabels.push_back(scalingProfile->timebaseSteps[i].label);
        }
        for (size_t i = 0U; i < scalingProfile->voltageStepCount; ++i) {
            voltageScaleLabels.push_back(
                scalingProfile->voltageSteps[i].label
            );
        }
        if (timebase >= static_cast<int>(timebaseLabels.size())) {
            timebase = static_cast<int>(timebaseLabels.size()) - 1;
        }
        for (int channel = 0; channel < 2; ++channel) {
            if (
                voltsPerDivision[channel] >=
                    static_cast<int>(voltageScaleLabels.size())
            ) {
                voltsPerDivision[channel] =
                    static_cast<int>(voltageScaleLabels.size()) - 1;
            }
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Horizontal");
        ImGui::Combo(
            "Timebase",
            &timebase,
            timebaseLabels.data(),
            static_cast<int>(timebaseLabels.size())
        );
        ImGui::Separator();
        for (int channel = 0; channel < 2; ++channel) {
            ImGui::PushID(channel);
            ImGui::Text("Channel %d", channel + 1);
            ImGui::Checkbox("Enabled", &channelEnabled[channel]);
            ImGui::Combo(
                "Scale",
                &voltsPerDivision[channel],
                voltageScaleLabels.data(),
                static_cast<int>(voltageScaleLabels.size())
            );
            ImGui::PopID();
        }
        if (ImGui::Button("Set zero", ImVec2(-1.0f, 0.0f))) {
            channelZeroReference[0] = channelBaselineMean[0];
            channelZeroReference[1] = channelBaselineMean[1];
        }
        ImGui::Separator();
        ImGui::TextUnformatted("Trigger");
        {
            const char *triggerSourceLabels[kTriggerSourceOptionCount];
            const char *triggerSlopeLabels[kTriggerSlopeOptionCount];
            const char *triggerModeLabels[kTriggerModeOptionCount];
            int triggerSourceIndex = static_cast<int>(triggerSource);
            int triggerSlopeIndex = static_cast<int>(triggerSlope);
            int triggerModeIndex = static_cast<int>(triggerMode);
            size_t optionIndex = 0U;

            for (optionIndex = 0U;
                 optionIndex < kTriggerSourceOptionCount; ++optionIndex) {
                triggerSourceLabels[optionIndex] =
                    kTriggerSourceOptions[optionIndex].label;
            }
            for (optionIndex = 0U;
                 optionIndex < kTriggerSlopeOptionCount; ++optionIndex) {
                triggerSlopeLabels[optionIndex] =
                    kTriggerSlopeOptions[optionIndex].label;
            }
            for (optionIndex = 0U;
                 optionIndex < kTriggerModeOptionCount; ++optionIndex) {
                triggerModeLabels[optionIndex] =
                    kTriggerModeOptions[optionIndex].label;
            }

            if (ImGui::Combo(
                    "Source", &triggerSourceIndex, triggerSourceLabels,
                    static_cast<int>(kTriggerSourceOptionCount))) {
                triggerSource =
                    kTriggerSourceOptions[triggerSourceIndex].source;
            }
            if (ImGui::Combo(
                    "Slope", &triggerSlopeIndex, triggerSlopeLabels,
                    static_cast<int>(kTriggerSlopeOptionCount))) {
                triggerSlope = kTriggerSlopeOptions[triggerSlopeIndex].slope;
            }
            ImGui::SliderInt("Level", &triggerLevel, 0, 255);
            ImGui::SliderFloat(
                "Position", &triggerPosition, 0.0f, 1.0f, "%.2f"
            );
            if (ImGui::Combo(
                    "Mode", &triggerModeIndex, triggerModeLabels,
                    static_cast<int>(kTriggerModeOptionCount))) {
                triggerMode = kTriggerModeOptions[triggerModeIndex].mode;
            }
            if (triggerMode == ETriggerMode::eSingle) {
                if (ImGui::Button("Rearm", ImVec2(-1.0f, 0.0f))) {
                    singleArmed = true;
                    heldValid = false;
                }
            }
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

        char waveformStatus[128];

        if (shownHasWaveform) {
            size_t triggerSampleIndex =
                shownTriggerFound
                    ? shownTriggerIndex
                    : static_cast<size_t>(latestTriggerPoint);

            if (triggerSampleIndex >= shownWaveform->sampleCount) {
                triggerSampleIndex = 0U;
            }

            const double channelOneVolts = sampleToVolts(
                shownWaveform->channelOne[triggerSampleIndex],
                scalingProfile->voltageSteps[voltsPerDivision[0]]
                    .valuePerDivision,
                static_cast<uint8_t>(channelZeroReference[0] + 0.5f),
                scalingProfile->adcCountsPerDivision
            );
            const double channelTwoVolts = sampleToVolts(
                shownWaveform->channelTwo[triggerSampleIndex],
                scalingProfile->voltageSteps[voltsPerDivision[1]]
                    .valuePerDivision,
                static_cast<uint8_t>(channelZeroReference[1] + 0.5f),
                scalingProfile->adcCountsPerDivision
            );
            size_t windowRelativeIndex = 0U;

            if (triggerSampleIndex > displayWindowStart) {
                windowRelativeIndex = triggerSampleIndex - displayWindowStart;
            }
            if (windowRelativeIndex >= displayWindowSamples) {
                windowRelativeIndex = displayWindowSamples - 1U;
            }

            const double triggerSeconds = sampleIndexToSeconds(
                windowRelativeIndex,
                displayWindowSamples,
                scalingProfile->timebaseSteps[timebase].valuePerDivision,
                scalingProfile->horizontalDivisions
            );

            snprintf(
                waveformStatus,
                sizeof(waveformStatus),
                "Waveform %zu samples (%s %zu) CH1 %.3fV CH2 %.3fV @ %.3gs",
                shownWaveform->sampleCount,
                shownTriggerFound
                    ? "trig"
                   : (triggerMode == ETriggerMode::eAuto ? "auto" : "wait"),
                triggerSampleIndex,
                channelOneVolts,
                channelTwoVolts,
                triggerSeconds
            );
        }
        else {
            snprintf(waveformStatus, sizeof(waveformStatus), "Waveform none");
        }

        ImGui::SetCursorScreenPos(statusPosition);
        ImGui::Text(
            "%s | %s | %s | CH1 %s | CH2 %s | %s",
            acquisitionRunning
                ? (acquisitionLoop.status.state.load() ==
                    EAcquisitionState::eRecovering
                    ? "Recovering USB connection" : "Acquiring")
                : "Stopped",
            demoMode ? "Demo mode" : "Live mode",
            deviceStatus.c_str(),
            channelEnabled[0] ? "on" : "off",
            channelEnabled[1] ? "on" : "off",
            waveformStatus
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

/** @fn resolveActiveScalingProfile */
static const SInstrumentScalingProfile* resolveActiveScalingProfile(
    const SUsbDeviceInfo &connectedDevice,
    const SUsbScanResult &usbScanResult
) {
    EInstrumentModel model = connectedDevice.model;

    if (model == EInstrumentModel::eUnknown) {
        if (!usbScanResult.devices.empty()) {
            model = usbScanResult.devices.front().model;
        }
        else {
            model = EInstrumentModel::eHantekDso2250;
        }
    }

    const SInstrumentScalingProfile *profile = findInstrtScalingProfile(model);

    if (profile == NULL) {
        profile = findInstrtScalingProfile(EInstrumentModel::eHantekDso2250);
    }

    return profile;
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
        case EAcquisitionOperation::eBeginCmd:
            status += " while beginning command";
            break;
        case EAcquisitionOperation::eSpeedBeforeCmd:
        case EAcquisitionOperation::eSpeedBeforeResponse:
            status += " while checking connection speed";
            break;
        case EAcquisitionOperation::eCaptureStateCmd:
            status += " while sending capture-state command";
            break;
        case EAcquisitionOperation::eCaptureStateResponse:
            status += " while reading capture state";
            break;
        case EAcquisitionOperation::eChannelDataCmd:
            status += " while requesting channel data";
            break;
        case EAcquisitionOperation::eChannelDataResponse:
            status += " while reading channel data";
            break;
        case EAcquisitionOperation::eCaptureStartCmd:
            status += " while starting capture";
            break;
        case EAcquisitionOperation::eTriggerEnabledCmd:
            status += " while enabling trigger";
            break;
        case EAcquisitionOperation::eForceTriggerCmd:
            status += " while forcing trigger";
            break;
        case EAcquisitionOperation::eSetFilterCmd:
            status += " while setting filters";
            break;
        case EAcquisitionOperation::eSetTriggerNSampleRateCmd:
            status += " while setting trigger/sample rate";
            break;
        case EAcquisitionOperation::eSetVoltageNCouplingCmd:
            status += " while setting voltage/coupling";
            break;
        case EAcquisitionOperation::eSetRelaysCmd:
            status += " while setting input relays";
            break;
        case EAcquisitionOperation::eGetChannelLevelCmd:
            status += " while reading channel-level calibration";
            break;
        case EAcquisitionOperation::eSetOffsetCmd:
            status += " while setting channel/trigger offset";
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
/*----------------------------------------------------------------------------*/

/** @fn computeDisplayWindow */
static size_t computeDisplayWindow(size_t sampleCount) {
    size_t windowSamples = sampleCount;

    if (sampleCount > 2U) {
        windowSamples = static_cast<size_t>(
            static_cast<float>(sampleCount) * kDisplayWindowFraction + 0.5f
        );
        if (windowSamples < 2U) {
            windowSamples = 2U;
        }
        if (windowSamples > sampleCount) {
            windowSamples = sampleCount;
        }
    }

    return windowSamples;
}
/*----------------------------------------------------------------------------*/

/** @fn drawChannelWaveform */
static void drawChannelWaveform(
    ImDrawList *drawList,
    const ImVec2 &position,
    const ImVec2 &size,
    const uint8_t *samples,
    size_t sampleCount,
    size_t windowStart,
    size_t windowLength,
    const SInstrumentScalingProfile *profile,
    double zeroReference,
    ImU32 color
) {
    std::vector<ImVec2> points;
    const float centerY = position.y + size.y * 0.5f;
    size_t index = 0U;

    if ((samples != NULL) && (profile != NULL) && (windowLength > 1U) &&
        ((windowStart + windowLength) <= sampleCount)) {
        const float pixelsPerDivision =
            static_cast<float>(size.y / profile->verticalDivisions);

        points.reserve(windowLength);
        for (index = 0U; index < windowLength; ++index) {
            const size_t sampleIndex = windowStart + index;
            const double divisions =
                (static_cast<double>(samples[sampleIndex]) - zeroReference) /
                profile->adcCountsPerDivision;
            const float x = position.x + size.x *
                static_cast<float>(index) /
                static_cast<float>(windowLength - 1U);
            const float y = centerY -
                static_cast<float>(divisions) * pixelsPerDivision;

            points.push_back(ImVec2(x, y));
        }
        drawList->AddPolyline(
            points.data(),
            static_cast<int>(points.size()),
            color,
            ImDrawFlags_None,
            1.5f
        );
    }
}
/******************************************************************************/
