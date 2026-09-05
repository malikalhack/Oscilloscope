/**
 * @file    waveform_parser.h
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

#ifndef WAVEFORM_PARSER_H_
#define WAVEFORM_PARSER_H_

/******************************* Included files ******************************/
#include <array>
#include <stddef.h>
#include <stdint.h>

/********************************* Definitions ********************************/

namespace oscilloscope {
namespace capture {

/** @brief Maximum samples in a DSO-2250 two-channel capture */
static const size_t kWaveformMaxSampleCount = 32768U;

/** @brief Decoded capture state and normalized trigger sample position */
struct SCaptureStateResponse {
    uint32_t triggerPoint;  /**< Trigger position in the sample ring */
    uint8_t captureState;   /**< Device capture state byte */
};

/** @brief One decoded two-channel DSO-2250 waveform capture */
struct SWaveformSamples {
    std::array<uint8_t, kWaveformMaxSampleCount> channelOne; /**< CH1 */
    std::array<uint8_t, kWaveformMaxSampleCount> channelTwo; /**< CH2 */
    size_t sampleCount; /**< Valid sample count in both channel arrays */
};

/********************* Application Programming Interface *********************/

/**
 * @brief Decodes a capture-state response packet
 * @param[in] data Raw USB response data
 * @param[in] length Number of bytes in data
 * @param[out] response Destination for decoded state and trigger position
 * @returns True when the response contains the required four bytes
 */
bool parseCaptureStateResponse(
    const uint8_t *data,
    size_t length,
    SCaptureStateResponse *response
);

/**
 * @brief Decodes an interleaved waveform sample buffer
 * @param[in] data Raw USB capture data
 * @param[in] length Number of bytes in data
 * @param[in] sampleCount Expected samples in each channel
 * @param[in] channelCount Number of interleaved channels
 * @param[in] channelOneSecond True when each pair is CH2 then CH1
 * @param[out] samples Destination for decoded channel samples
 * @returns True when the complete expected buffer was decoded
 */
bool parseInterleavedWaveformSamples(
    const uint8_t *data,
    size_t length,
    size_t sampleCount,
    uint8_t channelCount,
    bool channelOneSecond,
    SWaveformSamples *samples
);

} // namespace capture
} // namespace oscilloscope
/******************************************************************************/
#endif //! WAVEFORM_PARSER_H_