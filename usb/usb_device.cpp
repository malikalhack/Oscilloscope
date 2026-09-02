/**
 * @file    usb_device.cpp
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

/******************************* Included files ******************************/
#include "usb_device.h"
#include <libusb-1.0/libusb.h>

/********************************* Definitions ********************************/

namespace oscilloscope {
namespace usb {

/** @brief Defines a supported model signature */
struct SSupportedDevice {
    uint16_t vendorId;
    uint16_t productId;
    const char* modelName;
};

/****************************** Module variables ******************************/

/** @brief Maps supported USB VID/PID pairs to model display names */
static const SSupportedDevice supported_devices[] = {
    {0x04B4U, 0x2250U, "Hantek DSO-2250"}
};

/***************************** Private prototypes *****************************/

/**
 * @brief Finds the supported-model entry for a USB VID/PID pair
 * @param[in] vendorId USB vendor identifier
 * @param[in] productId USB product identifier
 * @returns Matching model entry, or NULL when the device is unsupported
 */
static const SSupportedDevice* findSupportedDevice(
    uint16_t vendorId,
    uint16_t productId
);

/********************* Application Programming Interface *********************/

/** @fn enumerateSupportedDevices */
SUsbScanResult enumerateSupportedDevices() {
    libusb_context* context = NULL;
    libusb_device** deviceList = NULL;
    SUsbScanResult result = {EScanStatus::eSuccess, {}, ""};
    const int initializationResult = libusb_init(&context);

    if (initializationResult != LIBUSB_SUCCESS) {
        result.status = EScanStatus::eInitializationFailed;
        result.errorMessage = libusb_error_name(initializationResult);
    }
    else {
        const ssize_t deviceCount = libusb_get_device_list(context, &deviceList);

        if (deviceCount < 0) {
            result.status = EScanStatus::eEnumerationFailed;
            result.errorMessage = libusb_error_name(static_cast<int>(deviceCount));
        }
        else {
            for (ssize_t index = 0; index < deviceCount; ++index) {
                libusb_device_descriptor descriptor;
                const int descriptorResult = libusb_get_device_descriptor(
                    deviceList[index],
                    &descriptor
                );

                if (descriptorResult == LIBUSB_SUCCESS) {
                    const SSupportedDevice* supportedDevice = findSupportedDevice(
                        descriptor.idVendor,
                        descriptor.idProduct
                    );

                    if (supportedDevice != NULL) {
                        result.devices.push_back({
                            descriptor.idVendor,
                            descriptor.idProduct,
                            libusb_get_bus_number(deviceList[index]),
                            libusb_get_device_address(deviceList[index]),
                            supportedDevice->modelName
                        });
                    }
                }
            }
        }

        if (deviceList != NULL) {
            libusb_free_device_list(deviceList, 1);
        }
        libusb_exit(context);
    }

    return result;
}

/****************************** Private functions *****************************/

/** @fn findSupportedDevice */
static const SSupportedDevice* findSupportedDevice(
    const uint16_t vendorId,
    const uint16_t productId
) {
    const SSupportedDevice* result = NULL;
    const size_t deviceCount =
        sizeof(supported_devices) / sizeof(supported_devices[0]);

    for (size_t index = 0U; index < deviceCount; ++index) {
        if (
            (supported_devices[index].vendorId == vendorId) &&
            (supported_devices[index].productId == productId)
        ) {
            result = &supported_devices[index];
            break;
        }
    }

    return result;
}

} // namespace usb
} // namespace oscilloscope
/******************************************************************************/
