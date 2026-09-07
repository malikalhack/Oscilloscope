/**
 * @file    waveform_trigger.cpp
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

/******************************* Included files ******************************/
#include "waveform_trigger.h"

/********************* Application Programming Interface *********************/

/** @fn oscilloscope::core::findEdgeTrigger */
bool oscilloscope::core::findEdgeTrigger(
    const uint8_t *samples,
    size_t sampleCount,
    uint8_t level,
    ETriggerSlope slope,
    size_t startIndex,
    size_t *triggerIndex
) {
    bool ret_val = false;
    size_t index = 0U;

    if ((samples != NULL) && (triggerIndex != NULL) && (sampleCount > 1U)) {
        size_t start = startIndex;

        if (start < 1U) {
            start = 1U;
        }
        for (index = start; (index < sampleCount) && (!ret_val); ++index) {
            const bool rising =
                (samples[index - 1U] < level) && (samples[index] >= level);
            const bool falling =
                (samples[index - 1U] > level) && (samples[index] <= level);
            const bool matched =
                (slope == ETriggerSlope::eRising) ? rising : falling;

            if (matched) {
                *triggerIndex = index;
                ret_val = true;
            }
        }
    }

    return ret_val;
}
/******************************************************************************/
