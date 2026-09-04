/**
 * @file    firmware_loader.h
 * @version 0.2.3
 * @authors Anton Chernov
 * @date    2026-09-02
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

#ifndef FIRMWARE_LOADER_H_
#define FIRMWARE_LOADER_H_

/******************************* Included files *******************************/
#include <stdint.h>
#include <string>

/********************************* Definitions ********************************/

namespace oscilloscope {
namespace usb {

/** @brief Describes the outcome of an FX2 firmware upload */
enum class EFirmwareLoadStatus {
    eLoaded,           /**< fxload uploaded the firmware successfully */
    eHexFileNotFound,  /**< The firmware or loader .hex file is missing */
    eFxloadNotFound,   /**< The fxload utility is not available in PATH */
    eUploadFailed      /**< fxload ran but reported a failure */
};

/** @brief Locates the loader and firmware .hex files for one device model */
struct SFirmwarePaths {
    std::string loaderHexPath;   /**< Path to the second-stage loader .hex */
    std::string firmwareHexPath; /**< Path to the main firmware .hex */
};

/** @brief Holds the result of a firmware upload attempt */
struct SFirmwareLoadResult {
    EFirmwareLoadStatus status; /**< Upload outcome */
    std::string errorMessage;   /**< Details when applicable */
};

/********************* Application Programming Interface **********************/

/**
 * @brief Uploads RAM-resident FX2 firmware to a bare/bootloader-mode device
 * @param[in] busNumber USB bus number of the target device
 * @param[in] deviceAddress USB device address of the target device
 * @param[in] paths Loader and firmware .hex file locations
 * @returns Upload status and error message when applicable
 * @note The device performs a soft USB re-enumeration once firmware starts
 *       running; the caller must re-scan for the device afterward, since its
 *       bus address may change.
 */
SFirmwareLoadResult loadFx2Firmware(
    uint8_t busNumber,
    uint8_t deviceAddress,
    const SFirmwarePaths &paths
);

} // namespace usb
} // namespace oscilloscope

#endif //! FIRMWARE_LOADER_H_
/******************************************************************************/
