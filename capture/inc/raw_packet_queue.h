/**
 * @file    raw_packet_queue.h
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

#ifndef RAW_PACKET_QUEUE_H_
#define RAW_PACKET_QUEUE_H_

/******************************* Included files ******************************/
#include <array>
#include <condition_variable>
#include <stddef.h>
#include <stdint.h>
#include <mutex>
#include <vector>

/********************************* Definitions ********************************/

namespace oscilloscope {
namespace capture {

/** @brief Maximum raw two-channel USB capture size accepted by the queue */
static const size_t RAW_USB_PACKET_MAX_SIZE = 65536U;

/** @brief Owns one raw USB response and its valid byte count */
struct SRawUsbPacket {
    std::array<uint8_t, RAW_USB_PACKET_MAX_SIZE> payload; /**< Raw bytes */
    size_t validLength; /**< Number of valid bytes in payload */
};

/** @brief Provides a bounded thread-safe FIFO for raw USB responses */
class RawPacketQueue {
public:
    /**
     * @brief Creates an empty queue with fixed packet capacity
     * @param[in] capacity Maximum number of retained packets
     */
    explicit RawPacketQueue(size_t capacity);

    /**
     * @brief Copies a raw response into the queue without blocking
     * @param[in] data Raw response bytes
     * @param[in] length Number of bytes to copy
     * @returns True when the packet was accepted
     * @note When full, the oldest packet is discarded before insertion.
     */
    bool push(const uint8_t *data, size_t length);

    /**
     * @brief Waits for and removes the oldest retained packet
     * @param[out] packet Destination receiving the removed packet
     * @returns True when a packet was returned, false when closed and empty
     */
    bool waitPop(SRawUsbPacket *packet);

    /** @brief Closes the queue and wakes every waiting consumer */
    void close();

    /** @brief Reopens and clears the queue for a new acquisition run */
    void reset();

    /**
     * @brief Reads the number of packets discarded since the last reset
     * @returns Number of packets discarded by the overflow policy
     */
    size_t getDroppedPacketCount() const;

private:
    std::vector<SRawUsbPacket> packets;
    mutable std::mutex mutex;
    std::condition_variable packetAvailable;
    size_t readIndex;
    size_t writeIndex;
    size_t packetCount;
    size_t droppedPacketCount;
    bool closed;
};

} // namespace capture
} // namespace oscilloscope
/******************************************************************************/
#endif //! RAW_PACKET_QUEUE_H_