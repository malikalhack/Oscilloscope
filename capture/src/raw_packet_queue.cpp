/**
 * @file    raw_packet_queue.cpp
 * @version 0.2.6
 * @authors Anton Chernov
 * @date    2026-09-04
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
#include <algorithm>

#include "raw_packet_queue.h"

/********************************* Definitions ********************************/

namespace oscilloscope {
namespace capture {

/********************* Application Programming Interface *********************/

/** @fn RawPacketQueue::RawPacketQueue(size_t capacity) */
RawPacketQueue::RawPacketQueue(const size_t capacity) :
    packets(capacity),
    readIndex(0U),
    writeIndex(0U),
    packetCount(0U),
    droppedPacketCount(0U),
    closed(false) {
}

/*----------------------------------------------------------------------------*/

/** @fn bool RawPacketQueue::push(const uint8_t *data, size_t length) */
bool RawPacketQueue::push(const uint8_t *data, const size_t length) {
    bool accepted = false;
    std::lock_guard<std::mutex> lock(mutex);

    if (
        !closed &&
        !packets.empty() &&
        (data != NULL) &&
        (length > 0U) &&
        (length <= kRawUsbPacketMaxSize)
    ) {
        if (packetCount == packets.size()) {
            readIndex = (readIndex + 1U) % packets.size();
            --packetCount;
            ++droppedPacketCount;
        }

        std::copy(data, data + length, packets[writeIndex].payload.begin());
        packets[writeIndex].validLength = length;
        writeIndex = (writeIndex + 1U) % packets.size();
        ++packetCount;
        accepted = true;
        packetAvailable.notify_one();
    }

    return accepted;
}

/*----------------------------------------------------------------------------*/

/** @fn bool RawPacketQueue::waitPop(SRawUsbPacket *packet) */
bool RawPacketQueue::waitPop(SRawUsbPacket *packet) {
    bool packetReturned = false;
    std::unique_lock<std::mutex> lock(mutex);

    packetAvailable.wait(lock, [this]() {
        return (packetCount > 0U) || closed;
    });

    if ((packet != NULL) && (packetCount > 0U)) {
        *packet = packets[readIndex];
        readIndex = (readIndex + 1U) % packets.size();
        --packetCount;
        packetReturned = true;
    }

    return packetReturned;
}

/*----------------------------------------------------------------------------*/

/** @fn void RawPacketQueue::close() */
void RawPacketQueue::close() {
    std::lock_guard<std::mutex> lock(mutex);

    closed = true;
    packetAvailable.notify_all();
}

/*----------------------------------------------------------------------------*/

/** @fn void RawPacketQueue::reset() */
void RawPacketQueue::reset() {
    std::lock_guard<std::mutex> lock(mutex);

    readIndex = 0U;
    writeIndex = 0U;
    packetCount = 0U;
    droppedPacketCount = 0U;
    closed = false;
}

/*----------------------------------------------------------------------------*/

/** @fn size_t RawPacketQueue::getDroppedPacketCount() const */
size_t RawPacketQueue::getDroppedPacketCount() const {
    size_t result = 0U;
    std::lock_guard<std::mutex> lock(mutex);

    result = droppedPacketCount;

    return result;
}

} // namespace capture
} // namespace oscilloscope
/******************************************************************************/