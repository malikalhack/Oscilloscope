/**
 * @file    waveform_ring_buffer.cpp
 * @version 0.2.7
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
#include "waveform_ring_buffer.h"

/********************************* Definitions ********************************/

namespace oscilloscope {
namespace capture {

/********************* Application Programming Interface *********************/

/** @fn WaveformRingBuffer::WaveformRingBuffer(size_t capacity) */
WaveformRingBuffer::WaveformRingBuffer(const size_t capacity) :
    frames(capacity),
    readIndex(0U),
    writeIndex(0U),
    frameCount(0U),
    droppedFrameCount(0U) {
}

/*----------------------------------------------------------------------------*/

/** @fn bool WaveformRingBuffer::push(const SWaveformSamples&, uint32_t) */
bool WaveformRingBuffer::push(
    const SWaveformSamples &samples,
    const uint32_t triggerPoint
) {
    bool accepted = false;
    std::lock_guard<std::mutex> lock(mutex);

    if (!frames.empty()) {
        if (frameCount == frames.size()) {
            readIndex = (readIndex + 1U) % frames.size();
            --frameCount;
            ++droppedFrameCount;
        }

        frames[writeIndex].samples = samples;
        frames[writeIndex].triggerPoint = triggerPoint;
        writeIndex = (writeIndex + 1U) % frames.size();
        ++frameCount;
        accepted = true;
    }

    return accepted;
}

/*----------------------------------------------------------------------------*/

/** @fn bool WaveformRingBuffer::popLatest(SWaveformFrame *frame) */
bool WaveformRingBuffer::popLatest(SWaveformFrame *frame) {
    bool frameReturned = false;
    std::lock_guard<std::mutex> lock(mutex);

    if ((frame != NULL) && (frameCount > 0U)) {
        const size_t latestIndex =
            (readIndex + frameCount - 1U) % frames.size();

        *frame = frames[latestIndex];
        readIndex = writeIndex;
        frameCount = 0U;
        frameReturned = true;
    }

    return frameReturned;
}

/*----------------------------------------------------------------------------*/

/** @fn void WaveformRingBuffer::reset() */
void WaveformRingBuffer::reset() {
    std::lock_guard<std::mutex> lock(mutex);

    readIndex = 0U;
    writeIndex = 0U;
    frameCount = 0U;
    droppedFrameCount = 0U;
}

/*----------------------------------------------------------------------------*/

/** @fn size_t WaveformRingBuffer::getDroppedFrameCount() const */
size_t WaveformRingBuffer::getDroppedFrameCount() const {
    size_t result = 0U;
    std::lock_guard<std::mutex> lock(mutex);

    result = droppedFrameCount;

    return result;
}

} // namespace capture
} // namespace oscilloscope
/******************************************************************************/
