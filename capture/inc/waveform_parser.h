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
static const size_t WAVEFORM_MAX_SAMPLE_COUNT = 32768U;

/** @brief Decoded capture state and normalized trigger sample position */
struct SCaptureStateResponse {
    uint8_t captureState;   /**< Device capture state byte */
    uint32_t triggerPoint;  /**< Trigger position in the sample ring */
};

/** @brief One decoded two-channel DSO-2250 waveform capture */
struct SWaveformSamples {
    std::array<uint8_t, WAVEFORM_MAX_SAMPLE_COUNT> channelOne; /**< CH1 */
    std::array<uint8_t, WAVEFORM_MAX_SAMPLE_COUNT> channelTwo; /**< CH2 */
    size_t sampleCount; /**< Valid sample count in both channel arrays */
};

/********************* Application Programming Interface *********************/

/**
 * @brief Decodes the DSO-2250 capture-state response packet
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
 * @brief Decodes an interleaved two-channel DSO-2250 sample buffer
 * @param[in] data Raw USB capture data
 * @param[in] length Number of bytes in data
 * @param[in] sampleCount Expected samples in each channel
 * @param[out] samples Destination for decoded channel samples
 * @returns True when the complete expected two-channel buffer was decoded
 * @note The legacy protocol orders each pair as CH2 then CH1
 */
bool parseInterleavedWaveformSamples(
    const uint8_t *data,
    size_t length,
    size_t sampleCount,
    SWaveformSamples *samples
);

} // namespace capture
} // namespace oscilloscope
/******************************************************************************/
#endif //! WAVEFORM_PARSER_H_