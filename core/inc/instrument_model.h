/**
 * @file    instrument_model.h
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

#ifndef INSTRUMENT_MODEL_H_
#define INSTRUMENT_MODEL_H_

/********************************* Definitions ********************************/

namespace oscilloscope {
namespace core {

/**
 * @brief Identifies a physical instrument model
 * @details Stable across a model's USB identities (for example the DSO-2250
 *          bootloader and operational VID/PID pairs share one entry), so it
 *          can key model-specific data such as display scaling profiles.
 */
enum class EInstrumentModel {
    eUnknown,      /**< No device, or a device without a scaling profile */
    eHantekDso2250 /**< Hantek DSO-2250, bootloader or operational state */
};

} // namespace core
} // namespace oscilloscope
/******************************************************************************/
#endif //! INSTRUMENT_MODEL_H_
