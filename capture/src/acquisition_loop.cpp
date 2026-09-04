/**
 * @file    acquisition_loop.cpp
 * @version 0.2.6
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
#include <chrono>

#include "acquisition_loop.h"

/********************************* Definitions ********************************/

namespace oscilloscope {
namespace capture {

/****************************** Module variables ******************************/

/** @brief Hantek DSO-2250 bulk endpoint addresses */
static const uint8_t EP_BULK_OUT = 0x02U;
static const uint8_t EP_BULK_IN = 0x86U;
static const uint16_t EP_BULK_IN_MAX_PACKET_LEN = 512U;

/** @brief Vendor control requests used by the polling cycle */
static const uint8_t CONTROL_GETSPEED = 0xB2U;
static const uint8_t CONTROL_BEGINCOMMAND = 0xB3U;

/** @brief Length of the FX2 connection-speed control response */
static const uint16_t SPEED_RESPONSE_LEN = 10U;

/** @brief Protocol command byte for the "get capture state" request */
static const uint8_t CMD_GET_CAPTURE_STATE = 6U;

/** @brief Delay between poll cycles; also the effective host-activity rate */
static const unsigned int POLL_INTERVAL_MS = 100U;
static const unsigned int RECOVERY_INTERVAL_MS = 250U;
static const unsigned int TRANSFER_TIMEOUT_MS = 250U;
static const unsigned int TRANSFER_ATTEMPTS = 1U;
static const unsigned int MAX_CONSECUTIVE_FAILURES = 3U;

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
        loop->status.lastCaptureState.store(-1);
        loop->status.droppedPacketCount.store(0U);
        loop->status.state.store(EAcquisitionState::eRunning);
        loop->status.failedOperation.store(EAcquisitionOperation::eNone);
        loop->status.lastTransferStatus.store(
            usb::EUsbTransferStatus::eSuccess
        );
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
    uint8_t response[EP_BULK_IN_MAX_PACKET_LEN];
    usb::SUsbTransferResult transferResult = {
        usb::EUsbTransferStatus::eSuccess, 0, ""
    };
    EAcquisitionOperation failedOperation = EAcquisitionOperation::eNone;
    unsigned int consecutiveFailures = 0U;
    unsigned int delayMs = POLL_INTERVAL_MS;

    while (!loop->stopRequested.load()) {
        transferResult = executePollingTransaction(
            connection, response, &failedOperation
        );

        if (transferResult.status == usb::EUsbTransferStatus::eSuccess) {
            loop->rawPacketQueue.push(
                response,
                static_cast<size_t>(transferResult.transferredBytes)
            );
            loop->status.droppedPacketCount.store(
                loop->rawPacketQueue.getDroppedPacketCount()
            );
            loop->status.state.store(EAcquisitionState::eRunning);
            loop->status.failedOperation.store(EAcquisitionOperation::eNone);
            loop->status.lastTransferStatus.store(
                usb::EUsbTransferStatus::eSuccess
            );
            consecutiveFailures = 0U;
            delayMs = POLL_INTERVAL_MS;
        }
        else {
            loop->status.failedOperation.store(failedOperation);
            loop->status.lastTransferStatus.store(transferResult.status);
            ++consecutiveFailures;

            if (transferResult.status == usb::EUsbTransferStatus::eNoDevice) {
                loop->status.state.store(EAcquisitionState::eDeviceLost);
                break;
            }
            else if (consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
                loop->status.state.store(EAcquisitionState::eFailed);
                break;
            }
            else {
                loop->status.state.store(EAcquisitionState::eRecovering);
                delayMs = RECOVERY_INTERVAL_MS;
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

    while (loop->rawPacketQueue.waitPop(&packet)) {
        if (packet.validLength > 0U) {
            loop->status.lastCaptureState.store(
                static_cast<int>(packet.payload[0])
            );
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
    uint8_t beginCommandPayload[10] =
        {0x0FU, 0x03U, 0x03U, 0x03U, 0U, 0U, 0U, 0U, 0U, 0U};
    uint8_t speedBuffer[SPEED_RESPONSE_LEN];
    uint8_t captureStateCommand[2] = {CMD_GET_CAPTURE_STATE, 0U};
    usb::SUsbTransferResult result = {
        usb::EUsbTransferStatus::eSuccess, 0, ""
    };

    *failedOperation = EAcquisitionOperation::eBeginCommand;
    result = usb::controlWrite(
        connection,
        CONTROL_BEGINCOMMAND,
        beginCommandPayload,
        sizeof(beginCommandPayload),
        0U,
        0U,
        TRANSFER_TIMEOUT_MS,
        TRANSFER_ATTEMPTS
    );

    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        *failedOperation = EAcquisitionOperation::eSpeedBeforeCommand;
        result = readConnectionSpeed(connection, speedBuffer);
    }
    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        *failedOperation = EAcquisitionOperation::eCaptureStateCommand;
        result = usb::bulkWrite(
            connection,
            EP_BULK_OUT,
            captureStateCommand,
            sizeof(captureStateCommand),
            TRANSFER_TIMEOUT_MS,
            TRANSFER_ATTEMPTS
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
            EP_BULK_IN,
            response,
            EP_BULK_IN_MAX_PACKET_LEN,
            TRANSFER_TIMEOUT_MS,
            TRANSFER_ATTEMPTS,
            4
        );
    }
    if (result.status == usb::EUsbTransferStatus::eSuccess) {
        *failedOperation = EAcquisitionOperation::eNone;
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
        CONTROL_GETSPEED,
        speedBuffer,
        SPEED_RESPONSE_LEN,
        0U,
        0U,
        TRANSFER_TIMEOUT_MS,
        TRANSFER_ATTEMPTS,
        1U
    );
}

} // namespace capture
} // namespace oscilloscope
/******************************************************************************/
