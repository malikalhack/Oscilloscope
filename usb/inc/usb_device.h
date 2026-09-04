/**
 * @file    usb_device.h
 * @version 0.2.2
 * @authors Anton Chernov
 * @date    2026-09-01
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
    eFirmwareLoadFailed,    /**< FX2 firmware upload failed */
    eOpenFailed,            /**< libusb_open failed */
    eClaimInterfaceFailed,  /**< libusb_claim_interface failed */
    eSetInterfaceFailed,    /**< libusb_set_interface_alt_setting failed */
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

/** @brief Describes the outcome of a bulk endpoint transfer */
enum class EUsbTransferStatus {
    eSuccess,    /**< The full requested length was transferred */
    eTimeout,    /**< All retry attempts timed out */
    eNoDevice,   /**< The device was disconnected mid-transfer */
    eError       /**< Some other libusb error occurred */
};

/** @brief Holds the result of a bulk endpoint transfer */
struct SUsbTransferResult {
    EUsbTransferStatus status; /**< Transfer outcome */
    int transferredBytes;       /**< Bytes actually transferred */
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
 * @note Uploads the FX2 firmware first when the device is still in its bare
 *       bootloader state; see `usb/inc/firmware_loader.h`.
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

/**
 * @brief Writes to a bulk OUT endpoint, retrying on timeout
 * @param[in] connection Active connection to write through
 * @param[in] endpointAddress Bulk OUT endpoint address (e.g. 0x02)
 * @param[in] data Buffer to send
 * @param[in] length Number of bytes to send
 * @param[in] timeoutMs Per-attempt timeout in milliseconds
 * @param[in] attempts Number of attempts before giving up on timeout
 * @returns Transfer status, bytes transferred, and error message
 */
SUsbTransferResult bulkWrite(
    const SUsbConnection &connection,
    uint8_t endpointAddress,
    const uint8_t *data,
    int length,
    unsigned int timeoutMs = 500U,
    unsigned int attempts = 3U
);

/**
 * @brief Reads from a bulk IN endpoint, retrying on timeout
 * @param[in] connection Active connection to read from
 * @param[in] endpointAddress Bulk IN endpoint address (e.g. 0x86)
 * @param[out] buffer Buffer to receive into
 * @param[in] length Number of bytes to read
 * @param[in] timeoutMs Per-attempt timeout in milliseconds
 * @param[in] attempts Number of attempts before giving up on timeout
 * @returns Transfer status, bytes transferred, and error message
 */
SUsbTransferResult bulkRead(
    const SUsbConnection &connection,
    uint8_t endpointAddress,
    uint8_t *buffer,
    int length,
    unsigned int timeoutMs = 500U,
    unsigned int attempts = 3U
);

/**
 * @brief Sends a vendor control OUT transfer, retrying on timeout
 * @param[in] connection Active connection to write through
 * @param[in] request Vendor bRequest code
 * @param[in] data Buffer to send
 * @param[in] length Number of bytes to send
 * @param[in] value wValue field
 * @param[in] index wIndex field
 * @param[in] timeoutMs Per-attempt timeout in milliseconds
 * @param[in] attempts Number of attempts before giving up on timeout
 * @returns Transfer status, bytes transferred, and error message
 */
SUsbTransferResult controlWrite(
    const SUsbConnection &connection,
    uint8_t request,
    const uint8_t *data,
    uint16_t length,
    uint16_t value = 0U,
    uint16_t index = 0U,
    unsigned int timeoutMs = 500U,
    unsigned int attempts = 3U
);

/**
 * @brief Reads a vendor control IN transfer, retrying on timeout
 * @param[in] connection Active connection to read from
 * @param[in] request Vendor bRequest code
 * @param[out] buffer Buffer to receive into
 * @param[in] length Number of bytes to read
 * @param[in] value wValue field
 * @param[in] index wIndex field
 * @param[in] timeoutMs Per-attempt timeout in milliseconds
 * @param[in] attempts Number of attempts before giving up on timeout
 * @returns Transfer status, bytes transferred, and error message
 */
SUsbTransferResult controlRead(
    const SUsbConnection &connection,
    uint8_t request,
    uint8_t *buffer,
    uint16_t length,
    uint16_t value = 0U,
    uint16_t index = 0U,
    unsigned int timeoutMs = 500U,
    unsigned int attempts = 3U
);

} // namespace usb
} // namespace oscilloscope
/******************************************************************************/
#endif // USB_DEVICE_H_
