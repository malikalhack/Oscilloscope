/**
 * @file    waveform_scaling.cpp
 * @version 0.2.9
 * @authors Anton Chernov
 * @date    2026-09-06
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
#include "waveform_scaling.h"

/********************* Application Programming Interface *********************/

/** @fn oscilloscope::core::sampleToVolts */
double oscilloscope::core::sampleToVolts(
    uint8_t rawSample,
    double voltsPerDivision,
    uint8_t adcCenterValue,
    double adcCountsPerDivision
) {
    const double centeredCounts =
        static_cast<double>(rawSample) -
        static_cast<double>(adcCenterValue);

    return (centeredCounts / adcCountsPerDivision) * voltsPerDivision;
}
/*----------------------------------------------------------------------------*/

/** @fn oscilloscope::core::sampleIndexToSeconds */
double oscilloscope::core::sampleIndexToSeconds(
    size_t sampleIndex,
    size_t sampleCount,
    double secondsPerDivision,
    double horizontalDivisions
) {
    double ret_val = 0.0;

    if (sampleCount != 0U) {
        const double captureSeconds =
            secondsPerDivision * horizontalDivisions;

        ret_val =
            (static_cast<double>(sampleIndex) /
                static_cast<double>(sampleCount)) *
            captureSeconds;
    }

    return ret_val;
}
/******************************************************************************/
