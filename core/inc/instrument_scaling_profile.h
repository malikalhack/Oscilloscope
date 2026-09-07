/**
 * @file    instrument_scaling_profile.h
 * @version 0.2.12
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

#ifndef INSTRUMENT_SCALING_PROFILE_H_
#define INSTRUMENT_SCALING_PROFILE_H_

/******************************* Included files ******************************/
#include <stddef.h>
#include <stdint.h>

#include "instrument_model.h"

/********************************* Definitions ********************************/

namespace oscilloscope {
namespace core {

/** @brief One labeled per-division step of a timebase or voltage control */
struct SScaleStep {
    const char *label;       /**< Display text, for example "20 mV/div" */
    double valuePerDivision; /**< Seconds or volts represented by one division */
};

/** @brief Describes the display-scaling characteristics of an instrument model */
struct SInstrumentScalingProfile {
    const SScaleStep *timebaseSteps; /**< Selectable timebase steps */
    const SScaleStep *voltageSteps;  /**< Selectable voltage-scale steps */
    double horizontalDivisions;      /**< Grid divisions along the time axis */
    double verticalDivisions;        /**< Grid divisions along the voltage axis */
    double adcCountsPerDivision;     /**< Raw ADC counts spanning one division */
    size_t timebaseStepCount;        /**< Number of entries in timebaseSteps */
    size_t voltageStepCount;         /**< Number of entries in voltageSteps */
    EInstrumentModel model;          /**< Instrument model this profile describes */
    uint8_t adcCenterValue;          /**< Raw sample value that means zero volts */
};

/********************* Application Programming Interface *********************/

/**
 * @brief Finds the display-scaling profile for an instrument model
 * @param[in] model Instrument model identifier to look up
 * @returns Matching profile, or NULL when the model has no scaling profile
 */
const SInstrumentScalingProfile* findInstrtScalingProfile(
    EInstrumentModel model
);

} // namespace core
} // namespace oscilloscope
/******************************************************************************/
#endif //! INSTRUMENT_SCALING_PROFILE_H_
