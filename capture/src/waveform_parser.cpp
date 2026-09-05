/**
 * @file    waveform_parser.cpp
 * @version 0.2.6
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
#include "waveform_parser.h"

/***************************** Private prototypes *****************************/

namespace oscilloscope {
namespace capture {

/**
 * @brief Converts the device trigger position to its sample-ring position
 * @param[in] triggerPoint Trigger position reported by the device
 * @returns Trigger position used by the legacy waveform buffer
 */
static uint32_t normalizeTriggerPoint(uint32_t triggerPoint);

/********************* Application Programming Interface *********************/

/** @fn parseCaptureStateResponse */
bool parseCaptureStateResponse(
    const uint8_t *data,
    size_t length,
    SCaptureStateResponse *response
) {
    bool result = false;
    uint32_t triggerPoint = 0U;

    if ((data != NULL) && (response != NULL) && (length >= 4U)) {
        triggerPoint =
            (static_cast<uint32_t>(data[1]) << 16U) |
            (static_cast<uint32_t>(data[3]) << 8U) |
            static_cast<uint32_t>(data[2]);
        response->captureState = data[0];
        response->triggerPoint = normalizeTriggerPoint(triggerPoint);
        result = true;
    }

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn parseInterleavedWaveformSamples */
bool parseInterleavedWaveformSamples(
    const uint8_t *data,
    size_t length,
    size_t sampleCount,
    SWaveformSamples *samples
) {
    bool result = false;
    size_t sampleIndex = 0U;
    size_t expectedLength = 0U;

    if (sampleCount <= kWaveformMaxSampleCount) {
        expectedLength = sampleCount * 2U;
        if (
            (data != NULL) &&
            (samples != NULL) &&
            (length == expectedLength)
        ) {
            for (sampleIndex = 0U; sampleIndex < sampleCount; ++sampleIndex) {
                samples->channelTwo[sampleIndex] = data[sampleIndex * 2U];
                samples->channelOne[sampleIndex] = data[sampleIndex * 2U + 1U];
            }
            samples->sampleCount = sampleCount;
            result = true;
        }
    }

    return result;
}

/****************************** Private functions *****************************/

/** @fn normalizeTriggerPoint */
static uint32_t normalizeTriggerPoint(uint32_t triggerPoint) {
    uint32_t upperBound = triggerPoint;
    uint32_t lowerBound = 0U;
    uint32_t offset = 0U;
    uint32_t midpoint = 0U;
    uint32_t highestPowerOfTwo = 1U;
    bool direction = true;
    bool result = true;

    while ((upperBound > 0U) && (highestPowerOfTwo <= (UINT32_MAX / 2U))) {
        upperBound >>= 1U;
        highestPowerOfTwo <<= 1U;
    }
    if (upperBound > 0U) {
        result = false;
    }
    if (result) {
        upperBound = highestPowerOfTwo - 1U;
        while (upperBound > lowerBound) {
            midpoint = (upperBound - lowerBound + 1U) / 2U + offset;
            if ((midpoint > triggerPoint) == direction) {
                if (!direction) {
                    offset = midpoint;
                }
                upperBound = (upperBound + lowerBound - 1U) / 2U;
                direction = true;
            }
            else {
                if (direction) {
                    offset = midpoint;
                }
                lowerBound = (lowerBound + upperBound + 1U) / 2U;
                direction = false;
            }
        }
    }

    return lowerBound;
}

} // namespace capture
} // namespace oscilloscope
/******************************************************************************/