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
#include <libusb-1.0/libusb.h>

#include "usb_device.h"

/********************************* Definitions ********************************/

namespace oscilloscope {
namespace usb {

/** @brief Defines a supported model signature */
struct SSupportedDevice {
    uint16_t vendorId;
    uint16_t productId;
    uint8_t interfaceNumber;
    const char *modelName;
};

/****************************** Module variables ******************************/

/** @brief Maps supported USB VID/PID pairs to model display names */
static const SSupportedDevice supported_devices[] = {
    {0x04B4U, 0x2250U, 0U, "Hantek DSO-2250"}
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

/**
 * @brief Finds a specific bus/address combination in the libusb device list
 * @param[in] deviceList libusb device list to search
 * @param[in] deviceInfo Requested device descriptor
 * @returns Matching libusb device, or NULL when not found
 */
static libusb_device* findDeviceByInfo(
    libusb_device **deviceList,
    const SUsbDeviceInfo &deviceInfo,
    const ssize_t deviceCount
);

/********************* Application Programming Interface *********************/

/** @fn enumerateSupportedDevices */
SUsbScanResult enumerateSupportedDevices() {
    libusb_context *context = NULL;
    libusb_device **deviceList = NULL;
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
                    const SSupportedDevice *supportedDevice = findSupportedDevice(
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

/** @fn connectToDevice */
SUsbConnectionResult connectToDevice(
    const SUsbDeviceInfo &deviceInfo,
    SUsbConnection *connection
) {
    SUsbConnectionResult result = {EConnectionStatus::eDisconnected, ""};
    libusb_context *context = NULL;
    libusb_device **deviceList = NULL;
    libusb_device *device = NULL;
    libusb_device_handle *handle = NULL;
    const SSupportedDevice *supportedDevice = NULL;
    ssize_t deviceCount = 0;
    int initializationResult = LIBUSB_SUCCESS;
    int openResult = LIBUSB_SUCCESS;
    int claimResult = LIBUSB_SUCCESS;

    if (connection == NULL) {
        result.status = EConnectionStatus::eDeviceNotFound;
        result.errorMessage = "Connection handle is null";
    }
    else {
        if (connection->isConnected) {
            result = disconnectFromDevice(connection);
        }

        if (result.status == EConnectionStatus::eDisconnected) {
            initializationResult = libusb_init(&context);

            if (initializationResult != LIBUSB_SUCCESS) {
                result.status = EConnectionStatus::eInitializationFailed;
                result.errorMessage = libusb_error_name(initializationResult);
            }
            else {
                deviceCount = libusb_get_device_list(context, &deviceList);
                device = findDeviceByInfo(deviceList, deviceInfo, deviceCount);
                supportedDevice = findSupportedDevice(
                    deviceInfo.vendorId,
                    deviceInfo.productId
                );

                if ((deviceCount < 0) || (device == NULL) ||
                    (supportedDevice == NULL)) {
                    result.status = EConnectionStatus::eDeviceNotFound;
                    result.errorMessage = "Requested USB device was not found";
                }
                else {
                    openResult = libusb_open(device, &handle);

                    if (openResult != LIBUSB_SUCCESS) {
                        result.status = EConnectionStatus::eOpenFailed;
                        result.errorMessage = libusb_error_name(openResult);
                    }
                    else {
                        claimResult = libusb_claim_interface(
                            handle,
                            supportedDevice->interfaceNumber
                        );

                        if (claimResult != LIBUSB_SUCCESS) {
                            result.status = EConnectionStatus::eClaimInterfaceFailed;
                            result.errorMessage = libusb_error_name(claimResult);
                        }
                        else {
                            connection->context = context;
                            connection->handle = handle;
                            connection->interfaceNumber =
                                supportedDevice->interfaceNumber;
                            connection->isConnected = true;
                            result.status = EConnectionStatus::eConnected;
                            context = NULL;
                            handle = NULL;
                        }
                    }
                }
            }
        }
    }

    if (deviceList != NULL) {
        libusb_free_device_list(deviceList, 1);
    }
    if (handle != NULL) {
        libusb_close(handle);
    }
    if (context != NULL) {
        libusb_exit(context);
    }

    return result;
}

/** @fn disconnectFromDevice */
SUsbConnectionResult disconnectFromDevice(SUsbConnection *connection) {
    SUsbConnectionResult result = {EConnectionStatus::eDisconnected, ""};
    int releaseResult = LIBUSB_SUCCESS;

    if (connection == NULL) {
        result.status = EConnectionStatus::eDeviceNotFound;
        result.errorMessage = "Connection handle is null";
    }
    else {
        if ((connection->handle != NULL) && connection->isConnected) {
            releaseResult = libusb_release_interface(
                connection->handle,
                connection->interfaceNumber
            );

            if (
                (releaseResult != LIBUSB_SUCCESS) &&
                (releaseResult != LIBUSB_ERROR_NO_DEVICE)
            ) {
                result.status = EConnectionStatus::eReleaseFailed;
                result.errorMessage = libusb_error_name(releaseResult);
            }
        }

        if (connection->handle != NULL) {
            libusb_close(connection->handle);
        }
        if (connection->context != NULL) {
            libusb_exit(connection->context);
        }

        connection->context = NULL;
        connection->handle = NULL;
        connection->interfaceNumber = 0U;
        connection->isConnected = false;
    }

    return result;
}

/****************************** Private functions *****************************/

/** @fn findSupportedDevice */
static const SSupportedDevice* findSupportedDevice(
    const uint16_t vendorId,
    const uint16_t productId
) {
    const SSupportedDevice *result = NULL;
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

/** @fn findDeviceByInfo */
static libusb_device* findDeviceByInfo(
    libusb_device **deviceList,
    const SUsbDeviceInfo &deviceInfo,
    const ssize_t deviceCount
) {
    libusb_device *result = NULL;
    libusb_device *candidate = NULL;
    libusb_device_descriptor descriptor;
    int descriptorResult = LIBUSB_ERROR_OTHER;

    for (ssize_t index = 0; index < deviceCount; ++index) {
        candidate = deviceList[index];
        descriptorResult = libusb_get_device_descriptor(
            candidate,
            &descriptor
        );

        if (descriptorResult == LIBUSB_SUCCESS) {
            if (
                (descriptor.idVendor == deviceInfo.vendorId) &&
                (descriptor.idProduct == deviceInfo.productId) &&
                (libusb_get_bus_number(candidate) == deviceInfo.busNumber) &&
                (libusb_get_device_address(candidate) == deviceInfo.deviceAddress)
            ) {
                result = candidate;
                break;
            }
        }
    }

    return result;
}

} // namespace usb
} // namespace oscilloscope
/******************************************************************************/
