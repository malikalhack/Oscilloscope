/**
 * @file    waveform_ring_buffer.h
 * @version 0.2.10
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

#ifndef WAVEFORM_RING_BUFFER_H_
#define WAVEFORM_RING_BUFFER_H_

/******************************* Included files *******************************/
#include <mutex>
#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "waveform_parser.h"

/********************************* Definitions ********************************/

namespace oscilloscope {
namespace capture {

/** @brief Number of decoded frames retained between capture and render */
static const size_t kWaveformRingBufferCapacity = 4U;

/** @brief One decoded waveform frame with its trigger metadata */
struct SWaveformFrame {
    SWaveformSamples samples; /**< Decoded two-channel sample buffer */
    uint32_t triggerPoint;    /**< Trigger position within the samples */
};

/**
 * @brief Provides a bounded thread-safe buffer of decoded waveform frames
 * @details Connects the capture processing thread, which pushes each
 * decoded frame, to render consumption, which only needs the freshest
 * frame and must never block waiting for one.
 */
class WaveformRingBuffer {
public:
    /**
     * @brief Creates an empty buffer with fixed frame capacity
     * @param[in] capacity Maximum number of retained frames
     */
    explicit WaveformRingBuffer(size_t capacity);

    /**
     * @brief Copies a decoded frame into the buffer without blocking
     * @param[in] samples Decoded waveform samples to store
     * @param[in] triggerPoint Trigger position associated with samples
     * @returns True when the frame was accepted
     * @note When full, the oldest frame is discarded before insertion.
     */
    bool push(const SWaveformSamples &samples, uint32_t triggerPoint);

    /**
     * @brief Removes every buffered frame and returns only the newest one
     * @param[out] frame Destination receiving the most recent frame
     * @returns True when a frame was returned, false when the buffer is empty
     */
    bool popLatest(SWaveformFrame *frame);

    /** @brief Clears the buffer for a new acquisition run */
    void reset();

    /**
     * @brief Reads the number of frames discarded since the last reset
     * @returns Number of frames discarded by the overflow policy
     */
    size_t getDroppedFrameCount() const;

private:
    std::vector<SWaveformFrame> frames;
    mutable std::mutex mutex;
    size_t readIndex;
    size_t writeIndex;
    size_t frameCount;
    size_t droppedFrameCount;
};

} // namespace capture
} // namespace oscilloscope
/******************************************************************************/
#endif //! WAVEFORM_RING_BUFFER_H_
