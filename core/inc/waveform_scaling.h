/**
 * @file    waveform_scaling.h
 * @version 0.2.8
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

/** @brief Oscilloscope grid divisions along the horizontal (time) axis */
static const double kHorizontalDivisions = 10.0;

/** @brief Oscilloscope grid divisions along the vertical (voltage) axis */
static const double kVerticalDivisions = 8.0;

/** @brief Raw sample value that represents zero volts */
static const uint8_t kAdcCenterValue = 128U;

/** @brief Raw ADC counts spanning one vertical division */
static const double kAdcCountsPerDivision = 256.0 / kVerticalDivisions;

/** @brief Seconds/division for each supported timebase selection */
static const double kTimebaseSecondsPerDivision[10] = {
    4.0e-9, 20.0e-9, 100.0e-9, 1.0e-6, 10.0e-6,
    100.0e-6, 1.0e-3, 10.0e-3, 100.0e-3, 1.0
};

/** @brief Volts/division for each supported voltage scale selection */
static const double kVoltageScaleVoltsPerDivision[8] = {
    0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0
};

/********************* Application Programming Interface *********************/

/**
 * @brief Converts a raw ADC sample to a signed voltage
 * @param[in] rawSample Raw 8-bit sample value from the capture buffer
 * @param[in] voltsPerDivision Selected voltage scale for the sample's channel
 * @returns The sample value in volts, centered on zero at the ADC midpoint
 */
double sampleToVolts(uint8_t rawSample, double voltsPerDivision);

/**
 * @brief Converts a sample index into elapsed capture time
 * @param[in] sampleIndex Zero-based index of the sample in the capture
 * @param[in] sampleCount Total number of samples spanning the capture
 * @param[in] secondsPerDivision Selected timebase for the capture
 * @returns The elapsed time in seconds, or zero when sampleCount is zero
 */
double sampleIndexToSeconds(
    size_t sampleIndex,
    size_t sampleCount,
    double secondsPerDivision
);

} // namespace core
} // namespace oscilloscope
/******************************************************************************/
#endif //! WAVEFORM_SCALING_H_
