/**
 * @file    waveform_trigger.h
 * @version 0.2.12
 * @authors Anton Chernov
 * @date    2026-09-07
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

#ifndef WAVEFORM_TRIGGER_H_
#define WAVEFORM_TRIGGER_H_

/******************************* Included files ******************************/
#include <stddef.h>
#include <stdint.h>

/********************************* Definitions ********************************/

namespace oscilloscope {
namespace core {

/** @brief Edge slope that qualifies as a trigger event */
enum class ETriggerSlope {
    eRising, /**< Trigger on a low-to-high level crossing */
    eFalling /**< Trigger on a high-to-low level crossing */
};

/********************* Application Programming Interface *********************/

/**
 * @brief Finds the first edge-trigger crossing in a sample buffer
 * @param[in] samples Raw ADC samples for the trigger source channel
 * @param[in] sampleCount Number of valid samples in @p samples
 * @param[in] level Raw ADC threshold that defines the trigger crossing
 * @param[in] slope Edge direction that qualifies as a trigger event
 * @param[in] startIndex First sample index to examine
 * @param[out] triggerIndex Destination for the crossing sample index
 * @returns True when a qualifying edge is found at or after @p startIndex
 */
bool findEdgeTrigger(
    const uint8_t *samples,
    size_t sampleCount,
    uint8_t level,
    ETriggerSlope slope,
    size_t startIndex,
    size_t *triggerIndex
);

} // namespace core
} // namespace oscilloscope
/******************************************************************************/
#endif //! WAVEFORM_TRIGGER_H_
