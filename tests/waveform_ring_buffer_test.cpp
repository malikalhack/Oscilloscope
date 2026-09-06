/**
 * @file    waveform_ring_buffer_test.cpp
 * @version 0.2.9
 * @authors Anton Chernov
 * @date    2026-09-05
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
#include <atomic>
#include <iostream>
#include <thread>

#include "waveform_ring_buffer.h"

/********************************* Definitions ********************************/

namespace {

using oscilloscope::capture::SWaveformFrame;
using oscilloscope::capture::SWaveformSamples;
using oscilloscope::capture::WaveformRingBuffer;

/***************************** Private prototypes *****************************/

static bool expect(bool condition, const char *message);
static SWaveformSamples makeSamples(uint8_t marker, size_t sampleCount);
static bool testPopLatestReturnsNewestWithoutOverflow();
static bool testOverflowDropsOldestAndCountsDrop();
static bool testResetClearsBufferAndDropCounter();
static bool testEmptyBufferPopLatestFails();
static bool testConcurrentProducerConsumer();

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

/** @fn makeSamples */
static SWaveformSamples makeSamples(
    const uint8_t marker,
    const size_t sampleCount
) {
    SWaveformSamples samples{};

    samples.channelOne[0] = marker;
    samples.sampleCount = sampleCount;

    return samples;
}

/*----------------------------------------------------------------------------*/

/** @fn testPopLatestReturnsNewestWithoutOverflow */
static bool testPopLatestReturnsNewestWithoutOverflow() {
    WaveformRingBuffer buffer(3U);
    SWaveformFrame frame;
    bool result = true;

    result = expect(
        buffer.push(makeSamples(1U, 100U), 11U), "push first frame"
    ) && result;
    result = expect(
        buffer.push(makeSamples(2U, 200U), 22U), "push second frame"
    ) && result;
    result = expect(buffer.popLatest(&frame), "pop latest frame") && result;
    result = expect(frame.samples.channelOne[0] == 2U, "newest marker") &&
        result;
    result = expect(frame.samples.sampleCount == 200U, "newest sample count")
        && result;
    result = expect(frame.triggerPoint == 22U, "newest trigger point") &&
        result;
    result = expect(!buffer.popLatest(&frame), "drained after pop latest") &&
        result;
    result = expect(buffer.getDroppedFrameCount() == 0U, "no drops") && result;

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn testOverflowDropsOldestAndCountsDrop */
static bool testOverflowDropsOldestAndCountsDrop() {
    WaveformRingBuffer buffer(2U);
    SWaveformFrame frame;
    bool result = true;

    result = expect(buffer.push(makeSamples(1U, 1U), 1U), "overflow push 1") &&
        result;
    result = expect(buffer.push(makeSamples(2U, 1U), 2U), "overflow push 2") &&
        result;
    result = expect(buffer.push(makeSamples(3U, 1U), 3U), "overflow push 3") &&
        result;
    result = expect(buffer.popLatest(&frame), "pop after overflow") && result;
    result = expect(frame.samples.channelOne[0] == 3U, "newest retained") &&
        result;
    result = expect(buffer.getDroppedFrameCount() == 1U, "drop counter") &&
        result;

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn testResetClearsBufferAndDropCounter */
static bool testResetClearsBufferAndDropCounter() {
    WaveformRingBuffer buffer(1U);
    SWaveformFrame frame;
    bool result = true;

    buffer.push(makeSamples(1U, 1U), 1U);
    buffer.push(makeSamples(2U, 1U), 2U);
    buffer.reset();
    result = expect(buffer.getDroppedFrameCount() == 0U, "reset drop counter")
        && result;
    result = expect(!buffer.popLatest(&frame), "reset clears old frames") &&
        result;
    result = expect(buffer.push(makeSamples(3U, 1U), 3U), "push after reset")
        && result;
    result = expect(buffer.popLatest(&frame), "pop after reset") && result;
    result = expect(frame.samples.channelOne[0] == 3U, "reset frame value") &&
        result;

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn testEmptyBufferPopLatestFails */
static bool testEmptyBufferPopLatestFails() {
    WaveformRingBuffer buffer(2U);
    SWaveformFrame frame;

    return expect(!buffer.popLatest(&frame), "empty buffer pop latest");
}

/*----------------------------------------------------------------------------*/

/** @fn testConcurrentProducerConsumer */
static bool testConcurrentProducerConsumer() {
    static const uint8_t kFrameTotal = 64U;
    WaveformRingBuffer buffer(4U);
    std::atomic<uint8_t> maxObserved{0U};
    std::atomic<bool> stopConsumer{false};
    std::thread consumer([&buffer, &maxObserved, &stopConsumer]() {
        SWaveformFrame frame;

        while (!stopConsumer.load()) {
            if (buffer.popLatest(&frame)) {
                if (frame.samples.channelOne[0] > maxObserved.load()) {
                    maxObserved.store(frame.samples.channelOne[0]);
                }
            }
        }
    });
    uint8_t marker = 0U;
    bool sawFinalFrame = false;
    SWaveformFrame frame;
    bool result = true;

    for (marker = 0U; marker < kFrameTotal; ++marker) {
        buffer.push(makeSamples(marker, marker), marker);
    }
    while (!sawFinalFrame) {
        if (
            buffer.popLatest(&frame) &&
            (frame.samples.channelOne[0] == (kFrameTotal - 1U))
        ) {
            sawFinalFrame = true;
        }
        else if (maxObserved.load() == (kFrameTotal - 1U)) {
            sawFinalFrame = true;
        }
        else {
            std::this_thread::yield();
        }
    }
    stopConsumer.store(true);
    consumer.join();
    result = expect(sawFinalFrame, "final frame observed by some consumer") &&
        result;

    return result;
}

} // namespace

/********************* Application Programming Interface *********************/

/** @fn main */
int main() {
    bool passed = true;
    int result = 1;

    passed = testPopLatestReturnsNewestWithoutOverflow() && passed;
    passed = testOverflowDropsOldestAndCountsDrop() && passed;
    passed = testResetClearsBufferAndDropCounter() && passed;
    passed = testEmptyBufferPopLatestFails() && passed;
    passed = testConcurrentProducerConsumer() && passed;
    if (passed) {
        result = 0;
    }

    return result;
}
/******************************************************************************/
