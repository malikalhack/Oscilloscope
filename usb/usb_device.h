/**
 * @file    usb_device.h
 * @version 0.2.1
 * @authors Anton Chernov
 * @date    2026-09-01
 * @date    @showdate "%Y-%m-%d"
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

#ifndef USB_DEVICE_H_
#define USB_DEVICE_H_

/******************************* Included files ******************************/
#include <stdint.h>
#include <string>
#include <vector>

#include <libusb-1.0/libusb.h>

/********************************* Definitions ********************************/

namespace oscilloscope {
namespace usb {

/** @brief Describes the outcome of a supported-device scan */
enum class EScanStatus {
    eSuccess,              /**< The scan completed successfully */
    eInitializationFailed, /**< libusb context initialization failed */
    eEnumerationFailed     /**< libusb device enumeration failed */
};

/** @brief Describes the outcome of a USB connection lifecycle operation */
enum class EConnectionStatus {
    eConnected,             /**< Device opened and interface claimed */
    eDisconnected,          /**< Device released and closed */
    eInitializationFailed,  /**< libusb context initialization failed */
    eDeviceNotFound,        /**< Matching device was not found */
    eOpenFailed,            /**< libusb_open failed */
    eClaimInterfaceFailed,  /**< libusb_claim_interface failed */
    eReleaseFailed          /**< libusb_release_interface failed */
};

/** @brief Identifies one supported USB oscilloscope instance */
struct SUsbDeviceInfo {
    uint16_t vendorId;     /**< USB vendor identifier */
    uint16_t productId;    /**< USB product identifier */
    uint8_t busNumber;     /**< USB bus number */
    uint8_t deviceAddress; /**< Address assigned on the USB bus */
    const char *modelName; /**< Supported model display name */
};

/** @brief Holds the outcome and matching devices from a USB scan */
struct SUsbScanResult {
    EScanStatus status;                  /**< Overall scan status */
    std::vector<SUsbDeviceInfo> devices; /**< All supported devices found */
    std::string errorMessage;            /**< libusb error for a failed scan */
};

/** @brief Describes an active USB connection to a supported device */
struct SUsbConnection {
    libusb_context *context;       /**< libusb connection context */
    libusb_device_handle *handle;  /**< Open USB device handle */
    uint8_t interfaceNumber;       /**< Claimed interface number */
    bool isConnected;              /**< True once the interface is claimed */
};

/** @brief Holds the result of a connection lifecycle operation */
struct SUsbConnectionResult {
    EConnectionStatus status;   /**< Operation outcome */
    std::string errorMessage;   /**< libusb error string when applicable */
};

/********************* Application Programming Interface *********************/

/**
 * @brief Enumerates every connected supported USB oscilloscope
 * @returns Scan status, matching devices, and an error message when applicable
 */
SUsbScanResult enumerateSupportedDevices();

/**
 * @brief Connects to a known supported USB oscilloscope
 * @param[in] deviceInfo Device descriptor returned by a scan result
 * @param[out] connection Connection state container that is filled in
 * @returns Connection status and error message when applicable
 */
SUsbConnectionResult connectToDevice(
    const SUsbDeviceInfo &deviceInfo,
    SUsbConnection *connection
);

/**
 * @brief Releases and closes a currently connected USB oscilloscope
 * @param[in,out] connection Connection state container to reset
 * @returns Disconnection status and error message when applicable
 */
SUsbConnectionResult disconnectFromDevice(SUsbConnection *connection);

} // namespace usb
} // namespace oscilloscope

#endif // USB_DEVICE_H_
/******************************************************************************/
