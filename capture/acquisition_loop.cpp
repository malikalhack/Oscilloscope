/**
 * @file    acquisition_loop.cpp
 * @version 0.2.3
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
#include <cstdio>
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

/** @brief Protocol command byte for the "get capture state" request */
static const uint8_t CMD_GET_CAPTURE_STATE = 6U;

/** @brief Delay between poll cycles; also the effective host-activity rate */
static const unsigned int POLL_INTERVAL_MS = 100U;

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

/********************* Application Programming Interface **********************/

/** @fn startAcquisitionLoop */
void startAcquisitionLoop(
    SAcquisitionLoop *loop,
    usb::SUsbConnection connection
) {
    if (loop != NULL) {
        loop->stopRequested.store(false);
        loop->status.pollCount.store(0UL);
        loop->status.errorCount.store(0UL);
        loop->status.lastCaptureState.store(-1);
        loop->workerThread = std::thread(pollCaptureState, loop, connection);
    }
}

/** @fn stopAcquisitionLoop */
void stopAcquisitionLoop(SAcquisitionLoop *loop) {
    if (loop != NULL) {
        loop->stopRequested.store(true);
        if (loop->workerThread.joinable()) {
            loop->workerThread.join();
        }
    }
}

/****************************** Private functions *****************************/

/** @fn pollCaptureState */
static void pollCaptureState(
    SAcquisitionLoop *loop,
    usb::SUsbConnection connection
) {
    uint8_t beginCommandPayload[10] =
        {0x0FU, 0x03U, 0x03U, 0x03U, 0U, 0U, 0U, 0U, 0U, 0U};
    uint8_t speedBuffer[10];
    uint8_t captureStateCommand[2] = {CMD_GET_CAPTURE_STATE, 0U};
    uint8_t response[EP_BULK_IN_MAX_PACKET_LEN];
    usb::SUsbTransferResult beginResult = {
        usb::EUsbTransferStatus::eError, 0, ""
    };
    usb::SUsbTransferResult speedResult = {
        usb::EUsbTransferStatus::eError, 0, ""
    };
    usb::SUsbTransferResult writeResult = {usb::EUsbTransferStatus::eError, 0, ""};
    usb::SUsbTransferResult readSpeedResult = {
        usb::EUsbTransferStatus::eError, 0, ""
    };
    usb::SUsbTransferResult readResult = {usb::EUsbTransferStatus::eError, 0, ""};

    while (!loop->stopRequested.load()) {
        beginResult = usb::controlWrite(
            connection,
            CONTROL_BEGINCOMMAND,
            beginCommandPayload,
            sizeof(beginCommandPayload)
        );
        speedResult = usb::controlRead(
            connection, CONTROL_GETSPEED, speedBuffer, sizeof(speedBuffer)
        );
        writeResult = usb::bulkWrite(
            connection,
            EP_BULK_OUT,
            captureStateCommand,
            sizeof(captureStateCommand)
        );
        readSpeedResult = usb::controlRead(
            connection, CONTROL_GETSPEED, speedBuffer, sizeof(speedBuffer)
        );
        readResult = usb::bulkRead(
            connection, EP_BULK_IN, response, EP_BULK_IN_MAX_PACKET_LEN
        );

        if (
            (beginResult.status == usb::EUsbTransferStatus::eSuccess) &&
            (speedResult.status == usb::EUsbTransferStatus::eSuccess) &&
            (writeResult.status == usb::EUsbTransferStatus::eSuccess) &&
            (readSpeedResult.status == usb::EUsbTransferStatus::eSuccess) &&
            (readResult.status == usb::EUsbTransferStatus::eSuccess)
        ) {
            loop->status.pollCount.fetch_add(1UL);
            loop->status.lastCaptureState.store(
                static_cast<int>(response[0])
            );
        }
        else {
            loop->status.errorCount.fetch_add(1UL);
            fprintf(
                stderr,
                "acquisition poll failed: begin=%s speed=%s write=%s "
                "read-speed=%s read=%s\n",
                beginResult.errorMessage.c_str(),
                speedResult.errorMessage.c_str(),
                writeResult.errorMessage.c_str(),
                readSpeedResult.errorMessage.c_str(),
                readResult.errorMessage.c_str()
            );
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(POLL_INTERVAL_MS)
        );
    }
}

} // namespace capture
} // namespace oscilloscope
/******************************************************************************/
