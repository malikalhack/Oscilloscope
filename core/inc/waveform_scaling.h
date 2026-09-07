/**
 * @file    waveform_scaling.h
 * @version 0.2.11
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

#ifndef WAVEFORM_SCALING_H_
#define WAVEFORM_SCALING_H_

/******************************* Included files ******************************/
#include <stddef.h>
#include <stdint.h>

/********************************* Definitions ********************************/

namespace oscilloscope {
namespace core {

/********************* Application Programming Interface *********************/

/**
 * @brief Converts a raw ADC sample to a signed voltage
 * @param[in] rawSample Raw 8-bit sample value from the capture buffer
 * @param[in] voltsPerDivision Selected voltage scale for the sample's channel
 * @param[in] adcCenterValue Raw sample value that represents zero volts
 * @param[in] adcCountsPerDivision Raw ADC counts spanning one vertical division
 * @returns The sample value in volts, centered on zero at the ADC midpoint
 */
double sampleToVolts(
    uint8_t rawSample,
    double voltsPerDivision,
    uint8_t adcCenterValue,
    double adcCountsPerDivision
);

/**
 * @brief Converts a sample index into elapsed capture time
 * @param[in] sampleIndex Zero-based index of the sample in the capture
 * @param[in] sampleCount Total number of samples spanning the capture
 * @param[in] secondsPerDivision Selected timebase for the capture
 * @param[in] horizontalDivisions Grid divisions spanned by the full capture
 * @returns The elapsed time in seconds, or zero when sampleCount is zero
 */
double sampleIndexToSeconds(
    size_t sampleIndex,
    size_t sampleCount,
    double secondsPerDivision,
    double horizontalDivisions
);

} // namespace core
} // namespace oscilloscope
/******************************************************************************/
#endif //! WAVEFORM_SCALING_H_
