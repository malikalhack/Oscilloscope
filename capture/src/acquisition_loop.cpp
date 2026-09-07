/**
 * @file    acquisition_loop.cpp
 * @version 0.2.12
 * @authors Anton Chernov
 * @date    2026-09-02
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

/******************************* Included files *******************************/
#include <array>
#include <chrono>

#include "acquisition_loop.h"
#include "waveform_parser.h"

/********************************* Definitions ********************************/

namespace oscilloscope {
namespace capture {

/****************************** Module variables ******************************/

/** @brief Vendor control requests used by the polling cycle */
static const uint8_t kControlGetSpeed = 0xB2U;
static const uint8_t kControlBeginCommand = 0xB3U;

/** @brief Length of the FX2 connection-speed control response */
static const uint16_t kSpeedResponseLen = 10U;

/** @brief Delay between poll cycles; also the effective host-activity rate */
static const unsigned int kPollIntervalMs = 100U;
static const unsigned int kRecoveryIntervalMs = 250U;
static const unsigned int kTransferTimeoutMs = 250U;
static const unsigned int kTransferAttempts = 1U;
static const unsigned int kMaxConsecutiveFailures = 3U;

/** @brief Raw capture-state value meaning "buffer empty" for every model */
static const uint8_t kCaptureEmptyState = 0U;

/**
 * @brief Consecutive empty polls tolerated before re-arming the trigger
 * @details A single force-trigger pulse can arrive before the capture
 * engine has re-armed, so the capture is retried periodically while the
 * buffer stays empty. TEMPORARY fixed threshold; Stage 5 will replace this
 * with real trigger-mode handling driven by the UI.
 */
static const unsigned int kForceRestartThreshold = 3U;

/**
 * @brief Fixed capture configuration sent once before the first capture
 * @details TEMPORARY until Stage 5 (see WorkingDocs/TECHNICAL_SPECIFICATION.md)
 * wires the Timebase/Scale/Trigger UI controls to the device: a single
 * known-safe configuration unblocks acquisition meanwhile: both channels
 * enabled at 5V/division AC coupling (capacitor-coupled inputs, safer
 * unattended default), 1ms/division, rising-edge auto-trigger on CH1.
 */
static const uint8_t kDefaultVoltageRangeCode = 0U;    /**< VOLTAGE_5V */
static const uint8_t kDefaultCouplingDc = 0U;          /**< COUPLING_AC */

/**
 * @brief DSO-2250 acquisition-configuration command group (0x0b-0x0f)
 * @details The DSO-2090 setTriggerNSampleRate command (0x01) does NOT arm
 * the DSO-2250 capture engine. This model needs the extended command group
 * 0x0b-0x0f - set channels, trigger source, record length, sample rate and
 * trigger position. Without it GetCaptureState never leaves its power-on
 * state and no waveform is ever returned. The byte values below reproduce
 * the known-good sequence captured from the vendor Windows driver (see
 * WorkingDocs USB captures): both channels, internal CH1 trigger, large
 * record buffer and a centered trigger position. TEMPORARY fixed
 * configuration until Stage 5 wires the Timebase/Scale/Trigger UI controls.
 */
static const uint8_t kDso2250SetTriggerSource[8] = {
    0x0CU, 0x0FU, 0x02U, 0x00U, 0x02U, 0x00U, 0x00U, 0x00U
};
/** Channel-enable command: both channels active */
static const uint8_t kDso2250SetChannels[4] = {
    0x0BU, 0x0FU, 0x00U, 0x00U
};
/** Record-length command: large capture buffer */
static const uint8_t kDso2250SetRecordLength[4] = {
    0x0DU, 0x0FU, 0x01U, 0x00U
};
/** Sample-rate command: default acquisition rate */
static const uint8_t kDso2250SetSampleRate[8] = {
    0x0EU, 0x00U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
};
/** Trigger-position command: centered post/pre-trigger window */
static const uint8_t kDso2250SetTriggerPosition[12] = {
    0x0FU, 0x00U, 0xFEU, 0xD7U, 0x07U, 0x00U,
    0xFEU, 0xFFU, 0x07U, 0x00U, 0x00U, 0x00U
};
/** GetLogicalData command: primes the logic/auto-range subsystem */
static const uint8_t kDso2250GetLogicalData[2] = {
    0x09U, 0x00U
};

/**
 * @brief Channel/trigger offset DAC configuration sent once before the
 * first capture
 * @details TEMPORARY until Stage 5 wires the vertical-position/trigger-
 * level UI controls: the offset DACs are seeded from the per-unit
 * calibration table (read once at connect) at their centered value, since
 * this application does not yet let the user move the trace or trigger
 * level away from the middle of the screen. Without any offset write the
 * device never leaves its power-on capture state.
 */
static const uint16_t kControlValueChannelLevel = 0x0008U;
/** 2 channels x 9 ranges x 2 (start,end) 16-bit entries */
static const uint16_t kChannelLevelTableBytes = 72U;
/** Calibration entry index for the active 5V/div, gain-factor-1 offset step
    (matches the same-device 5V/div capture that resets to DAC 77/68 counts) */
static const size_t kChannelLevelRangeIndex = 2U;
/** High-nibble marker for the CH1 offset DAC high byte */
static const uint8_t kOffsetDacMarkerCh1 = 0x20U;
/** High-nibble marker for the CH2 offset DAC high byte */
static const uint8_t kOffsetDacMarkerCh2 = 0x30U;
/** High-nibble marker for the trigger-level DAC high byte */
static const uint8_t kOffsetDacMarkerTrigger = 0x20U;
/** Centered trigger-level DAC value */
static const uint8_t kDefaultTriggerOffsetByte = 0x7FU;

/***************************** Private prototypes *****************************/

/**
 * @brief Background thread entry point that polls the capture state
 * @param[in,out] loop Loop control block to update
 * @param[in] connection USB connection to poll
 */
static void pollCaptureState(
    SAcquisitionLoop *loop,
    usb::SUsbConnection connection
);

/**
 * @brief Consumes queued USB responses without blocking the USB producer
 * @param[in,out] loop Loop control block containing the queue and status
 */
static void processRawPackets(SAcquisitionLoop *loop);

/**
 * @brief Executes one complete capture-state polling transaction
 * @param[in] connection USB connection to poll
 * @param[out] response Buffer receiving the capture-state response
 * @param[out] failedOperation First operation that failed
 * @returns Result of the first failed operation or the final successful read
 */
static usb::SUsbTransferResult executePollingTransaction(
    const usb::SUsbConnection &connection,
    uint8_t *response,
    EAcquisitionOperation *failedOperation
);

/**
 * @brief Reads a complete capture buffer after a successful trigger
 * @param[in] connection USB connection carrying the capture data
 * @param[out] data Destination for the complete interleaved capture
 * @param[out] failedOperation First operation that failed
 * @returns Result of the first failed operation or the final successful read
 */
static usb::SUsbTransferResult readCaptureData(
    const usb::SUsbConnection &connection,
    uint8_t *data,
    EAcquisitionOperation *failedOperation
);

/**
 * @brief Sends the fixed pre-capture configuration (filters, voltage
 * range/coupling relays, trigger source/slope, and sample rate)
 * @param[in] connection USB connection to configure
 * @param[out] failedOperation First operation that failed
 * @returns Result of the first failed operation or the final successful write
 * @note TEMPORARY: sends one hardcoded configuration; Stage 5 replaces this
 * with values derived from the Timebase/Scale/Trigger UI controls.
 */
static usb::SUsbTransferResult configureCapture(
    const usb::SUsbConnection &connection,
    EAcquisitionOperation *failedOperation
);

/**
 * @brief Sends the DSO-2250 acquisition-configuration command group
 * @details Primes the logic/auto-range subsystem with GetLogicalData
 * (0x09), draining its bulk-IN response, then issues the extended commands
 * 0x0b-0x0f that arm the DSO-2250 capture engine (channels, trigger source,
 * record length, sample rate, trigger position). Replaces the DSO-2090
 * setTriggerNSampleRate (0x01) command, which the DSO-2250 ignores.
 * @param[in] connection USB connection to configure
 * @param[out] failedOperation First operation that failed
 * @returns Result of the first failed operation or the final successful write
 */
static usb::SUsbTransferResult configureDso2250Timebase(
    const usb::SUsbConnection &connection,
    EAcquisitionOperation *failedOperation
);

/**
 * @brief Computes the centered offset DAC value for one channel
 * @param[in] channelLevels Calibration table read via the channel-level
 * control request (2 channels x 9 ranges x {start,end} 16-bit entries)
 * @param[in] channelIndex Channel index (0 = CH1, 1 = CH2)
 * @returns 12-bit calibration range midpoint DAC value for the 5V range
 */
static uint16_t channelLevelCenter(
    const uint8_t *channelLevels,
    uint8_t channelIndex
);

/**
 * @brief Starts a capture and enables its trigger
 * @param[in] connection USB connection controlling the capture
 * @param[out] failedOperation First operation that failed
 * @returns Result of the first failed operation or the final successful write
 */
static usb::SUsbTransferResult restartCapture(
    const usb::SUsbConnection &connection,
    EAcquisitionOperation *failedOperation
);

/**
 * @brief Executes a begin-command, speed-check, and bulk command transaction
 * @param[in] connection USB connection for the command
 * @param[in] command Command bytes to send
 * @param[in] commandLength Number of command bytes
 * @param[in] commandOperation Operation to report when the bulk write fails
 * @param[out] failedOperation First operation that failed
 * @returns Result of the first failed operation or the successful command write
 */
static usb::SUsbTransferResult executeCommand(
    const usb::SUsbConnection &connection,
    const uint8_t *command,
    int commandLength,
    EAcquisitionOperation commandOperation,
    EAcquisitionOperation *failedOperation
);

/**
 * @brief Reads the FX2 connection-speed response used to pace the protocol
 * @param[in] connection USB connection to read from
 * @param[out] speedBuffer Buffer receiving the speed response
 * @returns Transfer result of the control read
 */
static usb::SUsbTransferResult readConnectionSpeed(
    const usb::SUsbConnection &connection,
    uint8_t *speedBuffer
);

/********************* Application Programming Interface **********************/

/** @fn startAcquisitionLoop */
void startAcquisitionLoop(
    SAcquisitionLoop *loop,
    usb::SUsbConnection connection
) {
    if (loop != NULL) {
        loop->stopRequested.store(false);
        loop->rawPacketQueue.reset();
        loop->captureProtocol = connection.captureProtocol;
        loop->status.lastCaptureState.store(-1);
        loop->status.droppedPacketCount.store(0U);
        loop->status.state.store(EAcquisitionState::eRunning);
        loop->status.failedOperation.store(EAcquisitionOperation::eNone);
        loop->status.lastTransferStatus.store(
            usb::EUsbTransferStatus::eSuccess
        );
        loop->waveformRingBuffer.reset();
        loop->processingThread = std::thread(processRawPackets, loop);
        loop->workerThread = std::thread(pollCaptureState, loop, connection);
    }
}

/** @fn joinFinishedAcquisitionLoop */
bool joinFinishedAcquisitionLoop(SAcquisitionLoop *loop) {
    bool joined = false;
    EAcquisitionState state = EAcquisitionState::eStopped;

    if (loop != NULL) {
        state = loop->status.state.load();
        if (
            (state == EAcquisitionState::eDeviceLost) ||
            (state == EAcquisitionState::eFailed)
        ) {
            if (loop->workerThread.joinable()) {
                loop->workerThread.join();
            }
            loop->rawPacketQueue.close();
            if (loop->processingThread.joinable()) {
                loop->processingThread.join();
            }
            joined = true;
        }
    }

    return joined;
}

/*----------------------------------------------------------------------------*/

/** @fn getLatestWaveform */
bool getLatestWaveform(
    const SAcquisitionLoop *loop,
    SWaveformSamples *waveform,
    uint32_t *triggerPoint
) {
    bool result = false;
    SWaveformFrame frame;

    if (
        (loop != NULL) &&
        (waveform != NULL) &&
        (triggerPoint != NULL) &&
        loop->waveformRingBuffer.popLatest(&frame)
    ) {
        *waveform = frame.samples;
        *triggerPoint = frame.triggerPoint;
        result = true;
    }

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn stopAcquisitionLoop */
void stopAcquisitionLoop(SAcquisitionLoop *loop) {
    if (loop != NULL) {
        loop->stopRequested.store(true);
        loop->rawPacketQueue.close();
        if (loop->workerThread.joinable()) {
            loop->workerThread.join();
        }
        if (loop->processingThread.joinable()) {
            loop->processingThread.join();
        }
        loop->status.state.store(EAcquisitionState::eStopped);
    }
}

/****************************** Private functions *****************************/

/** @fn pollCaptureState */
static void pollCaptureState(
    SAcquisitionLoop *loop,
    usb::SUsbConnection connection
) {
    uint8_t response[kRawUsbPacketMaxSize];
    std::array<uint8_t, kRawUsbPacketMaxSize> captureData;
    usb::SUsbTransferResult transferResult = {
        usb::EUsbTransferStatus::eSuccess, 0, ""
    };
    EAcquisitionOperation failedOperation = EAcquisitionOperation::eNone;
    SCaptureStateResponse captureState = {0U, 0U};
    const size_t captureDataLength =
        connection.captureProtocol.sampleCount *
        connection.captureProtocol.channelCount;
    unsigned int consecutiveFailures = 0U;
    unsigned int delayMs = kPollIntervalMs;
    bool captureConfigured = false;
    bool captureStartRequired = true;
    unsigned int emptyCaptureCount = 0U;

    while (!loop->stopRequested.load()) {
        if (!captureConfigured) {
            transferResult = configureCapture(connection, &failedOperation);
            if (transferResult.status == usb::EUsbTransferStatus::eSuccess) {
                captureConfigured = true;
            }
        }
        else if (captureStartRequired) {
            transferResult = restartCapture(connection, &failedOperation);
            if (transferResult.status == usb::EUsbTransferStatus::eSuccess) {
                captureStartRequired = false;
            }
        }
        else {
            transferResult = executePollingTransaction(
                connection, response, &failedOperation
            );

            if (
                (transferResult.status == usb::EUsbTransferStatus::eSuccess) &&
                parseCaptureStateResponse(
                    response,
                    static_cast<size_t>(transferResult.transferredBytes),
                    &captureState
                )
            ) {
                loop->status.lastCaptureState.store(
                    static_cast<int>(captureState.captureState)
                );
                if (
                    captureState.captureState ==
                    connection.captureProtocol.captureCompleteState
                ) {
                    emptyCaptureCount = 0U;
                    transferResult = readCaptureData(
                        connection,
                        captureData.data(),
                        &failedOperation
                    );
                    if (
                        transferResult.status ==
                        usb::EUsbTransferStatus::eSuccess
                    ) {
                        loop->rawPacketQueue.push(
                            captureData.data(),
                            captureDataLength,
                            captureState.triggerPoint
                        );
                        transferResult = restartCapture(
                            connection,
                            &failedOperation
                        );
                    }
                }
                else if (captureState.captureState == kCaptureEmptyState) {
                    ++emptyCaptureCount;
                    if (emptyCaptureCount >= kForceRestartThreshold) {
                        emptyCaptureCount = 0U;
                        transferResult = restartCapture(
                            connection,
                            &failedOperation
                        );
                    }
                }
                else {
                    emptyCaptureCount = 0U;
                }
            }
        }

        if (transferResult.status == usb::EUsbTransferStatus::eSuccess) {
            loop->status.droppedPacketCount.store(
                loop->rawPacketQueue.getDroppedPacketCount()
            );
            loop->status.state.store(EAcquisitionState::eRunning);
            loop->status.failedOperation.store(EAcquisitionOperation::eNone);
            loop->status.lastTransferStatus.store(
                usb::EUsbTransferStatus::eSuccess
            );
            consecutiveFailures = 0U;
            delayMs = kPollIntervalMs;
        }
        else {
            loop->status.failedOperation.store(failedOperation);
            loop->status.lastTransferStatus.store(transferResult.status);
            ++consecutiveFailures;

            if (transferResult.status == usb::EUsbTransferStatus::eNoDevice) {
                loop->status.state.store(EAcquisitionState::eDeviceLost);
                break;
            }
            else if (consecutiveFailures >= kMaxConsecutiveFailures) {
                loop->status.state.store(EAcquisitionState::eFailed);
                break;
            }
            else {
                loop->status.state.store(EAcquisitionState::eRecovering);
                delayMs = kRecoveryIntervalMs;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }

    loop->rawPacketQueue.close();
}

/*----------------------------------------------------------------------------*/

/** @fn processRawPackets */
static void processRawPackets(SAcquisitionLoop *loop) {
    SRawUsbPacket packet;
    SWaveformSamples waveform;

    while (loop->rawPacketQueue.waitPop(&packet)) {
        if (
            parseInterleavedWaveformSamples(
                packet.payload.data(),
                packet.validLength,
                loop->captureProtocol.sampleCount,
                loop->captureProtocol.channelCount,
                loop->captureProtocol.channelOneSecond,
                &waveform
            )
        ) {
            loop->waveformRingBuffer.push(waveform, packet.triggerPoint);
        }
    }
}

/*----------------------------------------------------------------------------*/

/** @fn executePollingTransaction */
static usb::SUsbTransferResult executePollingTransaction(
    const usb::SUsbConnection &connection,
    uint8_t *response,
    EAcquisitionOperation *failedOperation
) {
    uint8_t speedBuffer[kSpeedResponseLen];
    uint8_t triggerEnabledCommand[2] = {
        connection.captureProtocol.triggerEnabledCmd, 0U
    };
    uint8_t captureStateCommand[2] = {
        connection.captureProtocol.captureStateCommand, 0U
    };
    usb::SUsbTransferResult result = {
        usb::EUsbTransferStatus::eSuccess, 0, ""
    };

    /* The device only reports a completed capture while its trigger stays
       armed; re-asserting this on every poll (not only after a restart)
       matches the real capture-state poll cadence observed on the wire. */
    result = executeCommand(
        connection,
        triggerEnabledCommand,
        sizeof(triggerEnabledCommand),
        EAcquisitionOperation::eTriggerEnabledCmd,
        failedOperation
    );

    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        result = executeCommand(
            connection,
            captureStateCommand,
            sizeof(captureStateCommand),
            EAcquisitionOperation::eCaptureStateCmd,
            failedOperation
        );
    }

    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        *failedOperation = EAcquisitionOperation::eSpeedBeforeResponse;
        result = readConnectionSpeed(connection, speedBuffer);
    }
    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        *failedOperation = EAcquisitionOperation::eCaptureStateResponse;
        result = usb::bulkRead(
            connection,
            connection.captureProtocol.bulkInEndpoint,
            response,
            connection.captureProtocol.bulkInPacketLength,
            kTransferTimeoutMs,
            kTransferAttempts,
            4
        );
    }
    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        *failedOperation = EAcquisitionOperation::eNone;
    }

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn readCaptureData */
static usb::SUsbTransferResult readCaptureData(
    const usb::SUsbConnection &connection,
    uint8_t *data,
    EAcquisitionOperation *failedOperation
) {
    uint8_t channelDataCommand[2] = {
        connection.captureProtocol.channelDataCommand, 0U
    };
    uint8_t speedBuffer[kSpeedResponseLen];
    usb::SUsbTransferResult result = {
        usb::EUsbTransferStatus::eSuccess, 0, ""
    };
    const size_t captureDataLength =
        connection.captureProtocol.sampleCount *
        connection.captureProtocol.channelCount;
    size_t capturePacketCount = 0U;
    size_t packetIndex = 0U;

    if (
        (connection.captureProtocol.bulkInPacketLength == 0U) ||
        (captureDataLength > kRawUsbPacketMaxSize) ||
        ((captureDataLength % connection.captureProtocol.bulkInPacketLength) !=
            0U)
    ) {
        result.status = usb::EUsbTransferStatus::eError;
        result.errorMessage = "Invalid capture protocol";
    }
    else {
        capturePacketCount =
            captureDataLength / connection.captureProtocol.bulkInPacketLength;
        result = executeCommand(
            connection,
            channelDataCommand,
            sizeof(channelDataCommand),
            EAcquisitionOperation::eChannelDataCmd,
            failedOperation
        );
    }
    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        *failedOperation = EAcquisitionOperation::eSpeedBeforeResponse;
        result = readConnectionSpeed(connection, speedBuffer);
    }
    for (
        packetIndex = 0U;
        (packetIndex < capturePacketCount) &&
        (result.status == usb::EUsbTransferStatus::eSuccess);
        ++packetIndex
    ) {
        *failedOperation = EAcquisitionOperation::eChannelDataResponse;
        result = usb::bulkRead(
            connection,
            connection.captureProtocol.bulkInEndpoint,
            data + packetIndex * connection.captureProtocol.bulkInPacketLength,
            connection.captureProtocol.bulkInPacketLength,
            kTransferTimeoutMs,
            kTransferAttempts,
            connection.captureProtocol.bulkInPacketLength
        );
    }
    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        *failedOperation = EAcquisitionOperation::eNone;
    }

    return result;
}
/*----------------------------------------------------------------------------*/

/** @fn channelLevelCenter */
static uint16_t channelLevelCenter(
    const uint8_t *channelLevels,
    uint8_t channelIndex
) {
    const size_t base = (
        ((static_cast<size_t>(channelIndex) * 9U) + kChannelLevelRangeIndex) *
        2U * 2U
    );
    const uint16_t offsetStart = static_cast<uint16_t>(
        (static_cast<uint16_t>(channelLevels[base]) << 8U) |
        channelLevels[base + 1U]
    );
    const uint16_t offsetEnd = static_cast<uint16_t>(
        (static_cast<uint16_t>(channelLevels[base + 2U]) << 8U) |
        channelLevels[base + 3U]
    );
    const uint32_t center = (
        static_cast<uint32_t>(offsetStart) + static_cast<uint32_t>(offsetEnd)
    ) / 2U;

    return static_cast<uint16_t>(center);
}

/*----------------------------------------------------------------------------*/

/** @fn configureDso2250Timebase */
static usb::SUsbTransferResult configureDso2250Timebase(
    const usb::SUsbConnection &connection,
    EAcquisitionOperation *failedOperation
) {
    struct SConfigCommand {
        const uint8_t *payload; /**< Command bytes */
        int length;             /**< Number of command bytes */
    };
    const SConfigCommand commands[5] = {
        {
            kDso2250SetTriggerSource,
            static_cast<int>(sizeof(kDso2250SetTriggerSource))
        },
        {
            kDso2250SetChannels,
            static_cast<int>(sizeof(kDso2250SetChannels))
        },
        {
            kDso2250SetRecordLength,
            static_cast<int>(sizeof(kDso2250SetRecordLength))
        },
        {
            kDso2250SetSampleRate,
            static_cast<int>(sizeof(kDso2250SetSampleRate))
        },
        {
            kDso2250SetTriggerPosition,
            static_cast<int>(sizeof(kDso2250SetTriggerPosition))
        }
    };
    uint8_t logicalData[kRawUsbPacketMaxSize];
    usb::SUsbTransferResult result = {
        usb::EUsbTransferStatus::eSuccess, 0, ""
    };
    size_t index = 0U;

    /* GetLogicalData (0x09) primes the logic/auto-range subsystem and
       returns one bulk-IN packet that the vendor driver drains before the
       analog arming group; the payload itself is unused here. */
    result = executeCommand(
        connection,
        kDso2250GetLogicalData,
        static_cast<int>(sizeof(kDso2250GetLogicalData)),
        EAcquisitionOperation::eSetTriggerNSampleRateCmd,
        failedOperation
    );
    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        *failedOperation = EAcquisitionOperation::eSetTriggerNSampleRateCmd;
        result = usb::bulkRead(
            connection,
            connection.captureProtocol.bulkInEndpoint,
            logicalData,
            connection.captureProtocol.bulkInPacketLength,
            kTransferTimeoutMs,
            kTransferAttempts,
            connection.captureProtocol.bulkInPacketLength
        );
    }

    for (index = 0U;
         (index < 5U) &&
         (result.status == usb::EUsbTransferStatus::eSuccess);
         ++index) {
        result = executeCommand(
            connection,
            commands[index].payload,
            commands[index].length,
            EAcquisitionOperation::eSetTriggerNSampleRateCmd,
            failedOperation
        );
    }

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn configureCapture */
static usb::SUsbTransferResult configureCapture(
    const usb::SUsbConnection &connection,
    EAcquisitionOperation *failedOperation
) {
    const uint8_t filterCommand[8] = {
        connection.captureProtocol.setFilterCmd, 0x0FU,
        0U, 0U, 0U, 0U, 0U, 0U
    };
    const uint8_t voltageByte = static_cast<uint8_t>(
        (2U - (kDefaultVoltageRangeCode % 3U)) |
        ((2U - (kDefaultVoltageRangeCode % 3U)) << 2U) |
        (3U << 4U)
    );
    const uint8_t voltageCommand[8] = {
        connection.captureProtocol.setVoltageNCouplingCmd, 0x0FU,
        voltageByte, 0U, 0U, 0U, 0U, 0U
    };
    /* Relay bitmap: index 3 and 6 flip to DC coupling for CH1/CH2 (base
       state is AC); the 5V range needs no attenuator relay change and CH1
       as trigger source needs no EXT relay change. */
    uint8_t relays[17] = {
        0x00U, 0x04U, 0x08U, 0x02U, 0x20U, 0x40U, 0x10U, 0x01U,
        0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
    };
    uint8_t channelLevels[kChannelLevelTableBytes];
    /* Offset DAC write: each channel and the trigger use a 12-bit DAC value
       stored as [marker | value>>8 (low nibble)] in the high byte and the
       value low byte next. CH1/CH2 values are seeded from the per-unit
       calibration table read below so the zero-volt level lands at
       mid-scale; the trigger level is centered (no calibration involved). */
    uint8_t offset[17] = {
        kOffsetDacMarkerCh1, 0U,
        kOffsetDacMarkerCh2, 0U,
        kOffsetDacMarkerTrigger, kDefaultTriggerOffsetByte,
        0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
    };
    usb::SUsbTransferResult result = {
        usb::EUsbTransferStatus::eSuccess, 0, ""
    };

    if (kDefaultCouplingDc != 0U) {
        relays[3] = static_cast<uint8_t>(~relays[3]);
        relays[6] = static_cast<uint8_t>(~relays[6]);
    }

    result = executeCommand(
        connection,
        filterCommand,
        sizeof(filterCommand),
        EAcquisitionOperation::eSetFilterCmd,
        failedOperation
    );
    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        result = configureDso2250Timebase(connection, failedOperation);
    }
    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        result = executeCommand(
            connection,
            voltageCommand,
            sizeof(voltageCommand),
            EAcquisitionOperation::eSetVoltageNCouplingCmd,
            failedOperation
        );
    }
    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        *failedOperation = EAcquisitionOperation::eSetRelaysCmd;
        result = usb::controlWrite(
            connection,
            connection.captureProtocol.setRelaysControlRequest,
            relays,
            sizeof(relays),
            0U,
            0U,
            kTransferTimeoutMs,
            kTransferAttempts
        );
    }
    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        *failedOperation = EAcquisitionOperation::eGetChannelLevelCmd;
        result = usb::controlRead(
            connection,
            connection.captureProtocol.controlCommandRequest,
            channelLevels,
            kChannelLevelTableBytes,
            kControlValueChannelLevel,
            0U,
            kTransferTimeoutMs,
            kTransferAttempts,
            kChannelLevelTableBytes
        );
    }
    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        const uint16_t centerChannelOne =
            channelLevelCenter(channelLevels, 0U);
        const uint16_t centerChannelTwo =
            channelLevelCenter(channelLevels, 1U);

        offset[0] = static_cast<uint8_t>(
            kOffsetDacMarkerCh1 | ((centerChannelOne >> 8U) & 0x0FU)
        );
        offset[1] = static_cast<uint8_t>(centerChannelOne & 0xFFU);
        offset[2] = static_cast<uint8_t>(
            kOffsetDacMarkerCh2 | ((centerChannelTwo >> 8U) & 0x0FU)
        );
        offset[3] = static_cast<uint8_t>(centerChannelTwo & 0xFFU);
        *failedOperation = EAcquisitionOperation::eSetOffsetCmd;
        result = usb::controlWrite(
            connection,
            connection.captureProtocol.setOffsetControlRequest,
            offset,
            sizeof(offset),
            0U,
            0U,
            kTransferTimeoutMs,
            kTransferAttempts
        );
    }
    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        result = configureDso2250Timebase(connection, failedOperation);
    }
    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        *failedOperation = EAcquisitionOperation::eNone;
    }

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn restartCapture */
static usb::SUsbTransferResult restartCapture(
    const usb::SUsbConnection &connection,
    EAcquisitionOperation *failedOperation
) {
    const uint8_t captureStartCmd[2] = {
        connection.captureProtocol.captureStartCmd, 0U
    };
    const uint8_t triggerEnabledCmd[2] = {
        connection.captureProtocol.triggerEnabledCmd, 0U
    };
    const uint8_t forceTriggerCmd[2] = {
        connection.captureProtocol.forceTriggerCmd, 0U
    };
    usb::SUsbTransferResult result = {
        usb::EUsbTransferStatus::eSuccess, 0, ""
    };

    result = executeCommand(
        connection,
        captureStartCmd,
        sizeof(captureStartCmd),
        EAcquisitionOperation::eCaptureStartCmd,
        failedOperation
    );
    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        result = executeCommand(
            connection,
            triggerEnabledCmd,
            sizeof(triggerEnabledCmd),
            EAcquisitionOperation::eTriggerEnabledCmd,
            failedOperation
        );
    }
    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        /* Free-running auto-trigger: this application has no manual
           trigger-source/level controls, so every capture is forced.
           Without this, the device stays armed and waiting for a real
           trigger edge that may never occur, and no capture ever
           reaches captureCompleteState. TEMPORARY until Stage 5 adds
           real trigger-mode selection. */
        result = executeCommand(
            connection,
            forceTriggerCmd,
            sizeof(forceTriggerCmd),
            EAcquisitionOperation::eForceTriggerCmd,
            failedOperation
        );
    }

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn executeCommand */
static usb::SUsbTransferResult executeCommand(
    const usb::SUsbConnection &connection,
    const uint8_t *command,
    const int commandLength,
    const EAcquisitionOperation commandOperation,
    EAcquisitionOperation *failedOperation
) {
    const uint8_t beginCommandPayload[10] =
        {0x0FU, 0x03U, 0x03U, 0x03U, 0U, 0U, 0U, 0U, 0U, 0U};
    uint8_t speedBuffer[kSpeedResponseLen];
    usb::SUsbTransferResult result = {
        usb::EUsbTransferStatus::eSuccess, 0, ""
    };

    *failedOperation = EAcquisitionOperation::eBeginCmd;
    result = usb::controlWrite(
        connection,
        kControlBeginCommand,
        beginCommandPayload,
        sizeof(beginCommandPayload),
        0U,
        0U,
        kTransferTimeoutMs,
        kTransferAttempts
    );
    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        *failedOperation = EAcquisitionOperation::eSpeedBeforeCmd;
        result = readConnectionSpeed(connection, speedBuffer);
    }
    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        *failedOperation = commandOperation;
        result = usb::bulkWrite(
            connection,
            connection.captureProtocol.bulkOutEndpoint,
            command,
            commandLength,
            kTransferTimeoutMs,
            kTransferAttempts
        );
    }

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn readConnectionSpeed */
static usb::SUsbTransferResult readConnectionSpeed(
    const usb::SUsbConnection &connection,
    uint8_t *speedBuffer
) {
    return usb::controlRead(
        connection,
        kControlGetSpeed,
        speedBuffer,
        kSpeedResponseLen,
        0U,
        0U,
        kTransferTimeoutMs,
        kTransferAttempts,
        1U
    );
}

} // namespace capture
} // namespace oscilloscope
/******************************************************************************/
