/**
 * @file    acquisition_loop.h
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

#ifndef ACQUISITION_LOOP_H_
#define ACQUISITION_LOOP_H_

/******************************* Included files ******************************/
#include <atomic>
#include <thread>

#include "raw_packet_queue.h"
#include "usb_device.h"

/********************************* Definitions ********************************/

namespace oscilloscope {
namespace capture {

/** @brief Describes the current state of the acquisition worker */
enum class EAcquisitionState {
    eStopped,    /**< The worker is not running */
    eRunning,    /**< Polling is proceeding normally */
    eRecovering, /**< A transient USB error is being retried */
    eDeviceLost, /**< The USB device was disconnected */
    eFailed      /**< The recovery limit was exhausted */
};

/** @brief Identifies the transfer that failed in a polling transaction */
enum class EAcquisitionOperation {
    eNone,                 /**< No transfer has failed */
    eBeginCommand,         /**< Begin-command control write */
    eSpeedBeforeCommand,   /**< Speed control read before command write */
    eCaptureStateCommand,  /**< Capture-state bulk command write */
    eSpeedBeforeResponse,  /**< Speed control read before response read */
    eCaptureStateResponse, /**< Capture-state bulk response read */
    eChannelDataCommand,   /**< Channel-data bulk command write */
    eChannelDataResponse,  /**< Channel-data bulk response read */
    eCaptureStartCommand,  /**< Capture-start bulk command write */
    eTriggerEnabledCommand /**< Trigger-enable bulk command write */
};

/** @brief Acquisition state safe to read from any thread */
struct SAcquisitionStatus {
    std::atomic<int> lastCaptureState{-1};       /**< Last raw capture state */
    std::atomic<size_t> droppedPacketCount{0U};  /**< Queue overflow count */
    std::atomic<EAcquisitionState> state{
        EAcquisitionState::eStopped
    }; /**< Current worker state */
    std::atomic<EAcquisitionOperation> failedOperation{
        EAcquisitionOperation::eNone
    }; /**< Most recent failed operation */
    std::atomic<usb::EUsbTransferStatus> lastTransferStatus{
        usb::EUsbTransferStatus::eSuccess
    }; /**< Most recent failed transfer status */
};

/** @brief Owns the background polling thread and its shared status */
struct SAcquisitionLoop {
    std::thread workerThread;               /**< Background USB producer */
    std::thread processingThread;           /**< Background packet consumer */
    RawPacketQueue rawPacketQueue{8U};      /**< Bounded raw response FIFO */
    std::atomic<bool> stopRequested{false}; /**< Set to request a stop */
    SAcquisitionStatus status;              /**< Shared poll status */
};

/********************* Application Programming Interface *********************/

/**
 * @brief Starts the background capture-state polling thread
 * @param[in,out] loop Loop control block to start; must not already be running
 * @param[in] connection Active USB connection to poll (copied for the thread)
 * @note This is the host activity that keeps the device's status LED
 *       blinking; there is no separate "ping" command in the protocol.
 */
void startAcquisitionLoop(
    SAcquisitionLoop *loop,
    usb::SUsbConnection connection
);

/**
 * @brief Requests the polling thread to stop and joins it
 * @param[in,out] loop Loop control block to stop
 * @note Must be called before disconnecting the underlying USB connection.
 */
void stopAcquisitionLoop(SAcquisitionLoop *loop);

/**
 * @brief Joins a worker that ended because of a terminal USB error
 * @param[in,out] loop Loop control block containing the finished worker
 * @returns True when a terminal worker was joined
 */
bool joinFinishedAcquisitionLoop(SAcquisitionLoop *loop);

} // namespace capture
} // namespace oscilloscope
/******************************************************************************/
#endif // ACQUISITION_LOOP_H_
