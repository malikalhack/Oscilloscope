/**
 * @file    instrument_scaling_profile.cpp
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

/******************************* Included files ******************************/
#include "instrument_scaling_profile.h"

/****************************** Module variables ******************************/

namespace oscilloscope {
namespace core {

/** @brief Timebase steps for the Hantek DSO-2250 */
static const SScaleStep kDso2250TimebaseSteps[] = {
    { "4 ns/div", 4.0e-9 }, { "20 ns/div", 20.0e-9 },
    { "100 ns/div", 100.0e-9 }, { "1 us/div", 1.0e-6 },
    { "10 us/div", 10.0e-6 }, { "100 us/div", 100.0e-6 },
    { "1 ms/div", 1.0e-3 }, { "10 ms/div", 10.0e-3 },
    { "100 ms/div", 100.0e-3 }, { "1 s/div", 1.0 }
};

/** @brief Voltage-scale steps for the Hantek DSO-2250 */
static const SScaleStep kDso2250VoltageSteps[] = {
    { "20 mV/div", 0.02 }, { "50 mV/div", 0.05 }, { "100 mV/div", 0.1 },
    { "200 mV/div", 0.2 }, { "500 mV/div", 0.5 }, { "1 V/div", 1.0 },
    { "2 V/div", 2.0 }, { "5 V/div", 5.0 }
};

/** @brief Display-scaling profile for every known instrument model */
static const SInstrumentScalingProfile kInstrScalingProfiles[] = {
    {
        kDso2250TimebaseSteps, kDso2250VoltageSteps,
        10.0, 8.0, 256.0 / 8.0,
        sizeof(kDso2250TimebaseSteps) / sizeof(kDso2250TimebaseSteps[0]),
        sizeof(kDso2250VoltageSteps) / sizeof(kDso2250VoltageSteps[0]),
        EInstrumentModel::eHantekDso2250, 128U
    }
};

/********************* Application Programming Interface *********************/

/** @fn oscilloscope::core::findInstrtScalingProfile */
const SInstrumentScalingProfile* findInstrtScalingProfile(
    EInstrumentModel model
) {
    const SInstrumentScalingProfile *result = NULL;
    const size_t profileCount =
        sizeof(kInstrScalingProfiles) / sizeof(kInstrScalingProfiles[0]);

    for (size_t index = 0U; index < profileCount; ++index) {
        if (kInstrScalingProfiles[index].model == model) {
            result = &kInstrScalingProfiles[index];
            break;
        }
    }

    return result;
}

} // namespace core
} // namespace oscilloscope
/******************************************************************************/
