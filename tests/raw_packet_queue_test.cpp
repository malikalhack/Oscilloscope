/**
 * @file    raw_packet_queue_test.cpp
 * @version 0.2.5
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
#include <iostream>
#include <thread>
#include <vector>

#include "raw_packet_queue.h"

/********************************* Definitions ********************************/

namespace {

using oscilloscope::capture::RawPacketQueue;
using oscilloscope::capture::SRawUsbPacket;

/***************************** Private prototypes *****************************/

static bool expect(const bool condition, const char *message);
static bool testFifoAndLength();
static bool testOverflowDropsOldest();
static bool testCloseWakesConsumer();
static bool testResetReopensQueue();
static bool testProducerConsumer();

/****************************** Private functions *****************************/

/** @fn expect */
static bool expect(const bool condition, const char *message) {
    bool result = condition;

    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
    }

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn testFifoAndLength */
static bool testFifoAndLength() {
    RawPacketQueue queue(3U);
    const uint8_t first[] = {1U, 2U};
    const uint8_t second[] = {3U, 4U, 5U};
    SRawUsbPacket packet;
    bool result = true;

    result = expect(
        queue.push(first, sizeof(first), 101U),
        "push first packet"
    ) &&
        result;
    result = expect(
        queue.push(second, sizeof(second), 202U),
        "push second packet"
    ) && result;
    result = expect(queue.waitPop(&packet), "pop first packet") && result;
    result = expect(packet.validLength == sizeof(first), "first length") &&
        result;
    result = expect(packet.triggerPoint == 101U, "first trigger point") &&
        result;
    result = expect(
        (packet.payload[0] == first[0]) &&
        (packet.payload[1] == first[1]),
        "first payload"
    ) && result;
    result = expect(queue.waitPop(&packet), "pop second packet") && result;
    result = expect(packet.validLength == sizeof(second), "second length") &&
        result;
    result = expect(packet.triggerPoint == 202U, "second trigger point") &&
        result;
    result = expect(
        (packet.payload[0] == second[0]) &&
        (packet.payload[1] == second[1]) &&
        (packet.payload[2] == second[2]),
        "second payload"
    ) && result;

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn testOverflowDropsOldest */
static bool testOverflowDropsOldest() {
    RawPacketQueue queue(2U);
    const uint8_t first = 1U;
    const uint8_t second = 2U;
    const uint8_t third = 3U;
    SRawUsbPacket packet;
    bool result = true;

    result = expect(queue.push(&first, 1U), "overflow push first") && result;
    result = expect(queue.push(&second, 1U), "overflow push second") && result;
    result = expect(queue.push(&third, 1U), "overflow push third") && result;
    queue.close();
    result = expect(queue.waitPop(&packet), "overflow pop second") && result;
    result = expect(packet.payload[0] == second, "oldest packet dropped") &&
        result;
    result = expect(queue.waitPop(&packet), "overflow pop third") && result;
    result = expect(packet.payload[0] == third, "newest packet retained") &&
        result;
    result = expect(!queue.waitPop(&packet), "closed empty queue") && result;
    result = expect(queue.getDroppedPacketCount() == 1U, "drop counter") &&
        result;

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn testCloseWakesConsumer */
static bool testCloseWakesConsumer() {
    RawPacketQueue queue(1U);
    bool popResult = true;
    std::thread consumer([&queue, &popResult]() {
        SRawUsbPacket packet;

        popResult = queue.waitPop(&packet);
    });
    bool result = true;

    queue.close();
    consumer.join();
    result = expect(!popResult, "close wakes an empty consumer") && result;

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn testResetReopensQueue */
static bool testResetReopensQueue() {
    RawPacketQueue queue(1U);
    const uint8_t first = 1U;
    const uint8_t second = 2U;
    SRawUsbPacket packet;
    bool result = true;

    queue.push(&first, 1U);
    queue.push(&second, 1U);
    queue.close();
    queue.reset();
    result = expect(queue.getDroppedPacketCount() == 0U, "reset drop counter")
        && result;
    result = expect(queue.push(&first, 1U), "push after reset") && result;
    result = expect(queue.waitPop(&packet), "pop after reset") && result;
    result = expect(packet.payload[0] == first, "reset clears old packets") &&
        result;

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn testProducerConsumer */
static bool testProducerConsumer() {
    static const size_t PACKET_TOTAL = 64U;
    RawPacketQueue queue(PACKET_TOTAL);
    std::vector<uint8_t> received;
    std::thread consumer([&queue, &received]() {
        SRawUsbPacket packet;

        while (queue.waitPop(&packet)) {
            received.push_back(packet.payload[0]);
        }
    });
    size_t packetIndex = 0U;
    bool result = true;

    for (packetIndex = 0U; packetIndex < PACKET_TOTAL; ++packetIndex) {
        const uint8_t value = static_cast<uint8_t>(packetIndex);

        result = expect(queue.push(&value, 1U), "concurrent push") && result;
    }
    queue.close();
    consumer.join();
    result = expect(received.size() == PACKET_TOTAL, "concurrent packet count")
        && result;
    for (packetIndex = 0U; packetIndex < received.size(); ++packetIndex) {
        result = expect(
            received[packetIndex] == static_cast<uint8_t>(packetIndex),
            "concurrent FIFO order"
        ) && result;
    }

    return result;
}

} // namespace

/********************* Application Programming Interface *********************/

/** @fn main */
int main() {
    bool passed = true;
    int result = 1;

    passed = testFifoAndLength() && passed;
    passed = testOverflowDropsOldest() && passed;
    passed = testCloseWakesConsumer() && passed;
    passed = testResetReopensQueue() && passed;
    passed = testProducerConsumer() && passed;
    if (passed) {
        result = 0;
    }

    return result;
}
/******************************************************************************/