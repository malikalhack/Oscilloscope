/**
 * @file    acquisition_loop.h
 * @version 0.2.2
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

#include "usb/usb_device.h"

/********************************* Definitions ********************************/

namespace oscilloscope {
namespace capture {

/** @brief Poll counters and last observed state, safe to read from any thread */
struct SAcquisitionStatus {
    std::atomic<unsigned long> pollCount{0UL};   /**< Successful poll cycles */
    std::atomic<unsigned long> errorCount{0UL};  /**< Failed poll cycles */
    std::atomic<int> lastCaptureState{-1};        /**< Last raw capture state */
};

/** @brief Owns the background polling thread and its shared status */
struct SAcquisitionLoop {
    std::thread workerThread;                 /**< Background polling thread */
    std::atomic<bool> stopRequested{false};   /**< Set to request a stop */
    SAcquisitionStatus status;                /**< Shared poll status */
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

} // namespace capture
} // namespace oscilloscope
/******************************************************************************/
#endif // ACQUISITION_LOOP_H_
