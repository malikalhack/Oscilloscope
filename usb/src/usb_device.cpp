/**
 * @file    usb_device.cpp
 * @version 0.2.6
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

/******************************* Included files *******************************/
#include <cstdlib>
#include <unistd.h>

#include <libusb-1.0/libusb.h>

#include "firmware_loader.h"
#include "usb_device.h"

/********************************* Definitions ********************************/

namespace oscilloscope {
namespace usb {

/** @brief Defines a supported model signature */
struct SSupportedDevice {
    uint16_t vendorId;
    uint16_t productId;
    uint16_t operationalVendorId;
    uint8_t interfaceNumber;
    uint8_t alternateSetting;
    bool requiresFirmware;
    const char *modelName;
    const char *firmwareBaseName;
};

/** @brief Delay between scans while waiting for FX2 re-enumeration */
static const unsigned int kFirmwareReenumerationPollDelayUs = 100000U;
/** @brief Maximum scans while waiting for the operational USB device */
static const unsigned int kFirmwareReenumerationAttempts = 50U;

/****************************** Module variables ******************************/

/** @brief Maps supported USB VID/PID pairs to model display names */
static const SSupportedDevice kSupportedDevices[] = {
    /* The bootloader exposes the bulk pair on alt setting 1. */
    {
        0x04B4U, 0x2250U, 0x04B5U, 0U, 1U, true,
        "Hantek DSO-2250 Bootloader", "DSO2250"
    },
    {
        0x04B5U, 0x2250U, 0x04B5U, 0U, 0U, false,
        "Hantek DSO-2250", "DSO2250"
    }
};

/***************************** Private prototypes *****************************/

/**
 * @brief Invokes a libusb transfer, retrying only while it times out
 * @tparam Transfer Callable returning the libusb transfer result code
 * @param[in] attempts Maximum attempts before giving up on a timeout
 * @param[in] transfer Callable performing one libusb transfer
 * @returns The libusb result code of the last attempt
 */
template <typename Transfer>
static int retryWhileTimeout(const unsigned int attempts, Transfer transfer) {
    int transferResult = LIBUSB_ERROR_TIMEOUT;
    unsigned int attempt = 0U;

    for (
        attempt = 0U;
        (attempt < attempts) && (transferResult == LIBUSB_ERROR_TIMEOUT);
        ++attempt
    ) {
        transferResult = transfer();
    }

    return transferResult;
}

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
 * @brief Finds a connected device by VID/PID and optional bus/address
 * @param[in] deviceList libusb device list to search
 * @param[in] deviceCount Number of entries in `deviceList`
 * @param[in] vendorId USB vendor identifier to match
 * @param[in] productId USB product identifier to match
 * @param[in] matchLocation Also require the bus/address to match when true
 * @param[in] busNumber USB bus number to match when `matchLocation` is true
 * @param[in] deviceAddress USB address to match when `matchLocation` is true
 * @returns Matching libusb device, or NULL when not found
 */
static libusb_device* findDevice(
    libusb_device **deviceList,
    const ssize_t deviceCount,
    uint16_t vendorId,
    uint16_t productId,
    bool matchLocation,
    uint8_t busNumber,
    uint8_t deviceAddress
);

/**
 * @brief Builds the loader/firmware .hex paths for a supported model
 * @param[in] supportedDevice Matching model entry
 * @returns Loader and firmware .hex file locations
 * @note The directory defaults to `firmware/` relative to the working
 *       directory, and can be overridden with `OSCILLOSCOPE_FIRMWARE_DIR`.
 *       When no override is set, a few common build/run working directories
 *       are tried automatically.
 */
static SFirmwarePaths resolveFirmwarePaths(
    const SSupportedDevice &supportedDevice
);

/**
 * @brief Maps a libusb bulk transfer result code to a transfer status
 * @param[in] transferResult Return value of `libusb_bulk_transfer`
 * @returns Classified transfer status
 */
static EUsbTransferStatus classifyBulkTransferResult(int transferResult);

/**
 * @brief Maps a libusb control transfer result code to a transfer status
 * @param[in] transferResult Return value of `libusb_control_transfer`
 *            (transferred byte count on success, negative error otherwise)
 * @returns Classified transfer status
 */
static EUsbTransferStatus classifyControlTransferResult(int transferResult);

/**
 * @brief Builds a transfer result and validates its minimum byte count
 * @param[in] status Status classified from the libusb return value
 * @param[in] transferResult Raw libusb return value for error reporting
 * @param[in] transferredBytes Number of bytes transferred
 * @param[in] minimumLength Minimum valid transfer length
 * @returns Complete transfer result for the public USB API
 */
static SUsbTransferResult makeTransferResult(
    EUsbTransferStatus status,
    int transferResult,
    int transferredBytes,
    int minimumLength
);

/********************* Application Programming Interface **********************/

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
/*----------------------------------------------------------------------------*/

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
    SFirmwareLoadResult firmwareResult = {EFirmwareLoadStatus::eLoaded, ""};
    ssize_t deviceCount = 0;
    int initializationResult = LIBUSB_SUCCESS;
    int openResult = LIBUSB_SUCCESS;
    int claimResult = LIBUSB_SUCCESS;
    unsigned int reenumerationAttempt = 0U;

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
                device = findDevice(
                    deviceList,
                    deviceCount,
                    deviceInfo.vendorId,
                    deviceInfo.productId,
                    true,
                    deviceInfo.busNumber,
                    deviceInfo.deviceAddress
                );
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
                    if (supportedDevice->requiresFirmware) {
                        firmwareResult = loadFx2Firmware(
                            deviceInfo.busNumber,
                            deviceInfo.deviceAddress,
                            resolveFirmwarePaths(*supportedDevice)
                        );
                    }

                    if (firmwareResult.status != EFirmwareLoadStatus::eLoaded) {
                        result.status = EConnectionStatus::eFirmwareLoadFailed;
                        result.errorMessage = firmwareResult.errorMessage;
                    }
                    else if (supportedDevice->requiresFirmware) {
                        /* The FX2 chip re-enumerates once firmware starts
                         * running, so the device pointer above is stale and
                         * its bus address may have changed. */
                        libusb_free_device_list(deviceList, 1);
                        deviceList = NULL;
                        device = NULL;

                        for (
                            reenumerationAttempt = 0U;
                            (reenumerationAttempt <
                                kFirmwareReenumerationAttempts) &&
                            (device == NULL);
                            ++reenumerationAttempt
                        ) {
                            usleep(kFirmwareReenumerationPollDelayUs);
                            deviceCount = libusb_get_device_list(
                                context,
                                &deviceList
                            );

                            if (deviceCount >= 0) {
                                device = findDevice(
                                    deviceList,
                                    deviceCount,
                                    supportedDevice->operationalVendorId,
                                    deviceInfo.productId,
                                    false,
                                    0U,
                                    0U
                                );

                                if (device != NULL) {
                                    supportedDevice = findSupportedDevice(
                                        supportedDevice->operationalVendorId,
                                        deviceInfo.productId
                                    );
                                }
                            }

                            if (device == NULL) {
                                if (deviceList != NULL) {
                                    libusb_free_device_list(deviceList, 1);
                                    deviceList = NULL;
                                }
                            }
                        }

                        if ((deviceCount < 0) || (device == NULL)) {
                            result.status = EConnectionStatus::eDeviceNotFound;
                            result.errorMessage =
                                "Device did not reappear after firmware upload";
                        }
                    }
                }

                if (
                    (result.status != EConnectionStatus::eDeviceNotFound) &&
                    (result.status != EConnectionStatus::eFirmwareLoadFailed)
                ) {
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
                            const int setInterfaceResult =
                                libusb_set_interface_alt_setting(
                                    handle,
                                    supportedDevice->interfaceNumber,
                                    supportedDevice->alternateSetting
                                );

                            if (setInterfaceResult != LIBUSB_SUCCESS) {
                                result.status = EConnectionStatus::eSetInterfaceFailed;
                                result.errorMessage =
                                    libusb_error_name(setInterfaceResult);
                                libusb_release_interface(
                                    handle,
                                    supportedDevice->interfaceNumber
                                );
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
/*----------------------------------------------------------------------------*/

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
/*----------------------------------------------------------------------------*/

/** @fn getConnectedDeviceInfo */
bool getConnectedDeviceInfo(
    const SUsbConnection &connection,
    SUsbDeviceInfo *deviceInfo
) {
    bool result = false;
    libusb_device *device = NULL;
    libusb_device_descriptor descriptor;
    const SSupportedDevice *supportedDevice = NULL;
    int descriptorResult = LIBUSB_ERROR_INVALID_PARAM;

    if (
        connection.isConnected &&
        (connection.handle != NULL) &&
        (deviceInfo != NULL)
    ) {
        device = libusb_get_device(connection.handle);
        descriptorResult = libusb_get_device_descriptor(device, &descriptor);
        if (descriptorResult == LIBUSB_SUCCESS) {
            supportedDevice = findSupportedDevice(
                descriptor.idVendor,
                descriptor.idProduct
            );
            if (supportedDevice != NULL) {
                *deviceInfo = {
                    descriptor.idVendor,
                    descriptor.idProduct,
                    libusb_get_bus_number(device),
                    libusb_get_device_address(device),
                    supportedDevice->modelName
                };
                result = true;
            }
        }
    }

    return result;
}
/*----------------------------------------------------------------------------*/

/** @fn bulkWrite */
SUsbTransferResult bulkWrite(
    const SUsbConnection &connection,
    const uint8_t endpointAddress,
    const uint8_t *data,
    const int length,
    const unsigned int timeoutMs,
    const unsigned int attempts
) {
    SUsbTransferResult result = {EUsbTransferStatus::eError, 0, ""};
    int transferredBytes = 0;
    const int transferResult = retryWhileTimeout(
        attempts,
        [&]() {
            return libusb_bulk_transfer(
                connection.handle,
                endpointAddress,
                const_cast<uint8_t*>(data),
                length,
                &transferredBytes,
                timeoutMs
            );
        }
    );

    result = makeTransferResult(
        classifyBulkTransferResult(transferResult),
        transferResult,
        transferredBytes,
        length
    );

    return result;
}
/*----------------------------------------------------------------------------*/

/** @fn bulkRead */
SUsbTransferResult bulkRead(
    const SUsbConnection &connection,
    const uint8_t endpointAddress,
    uint8_t *buffer,
    const int length,
    const unsigned int timeoutMs,
    const unsigned int attempts,
    const int minimumLength
) {
    SUsbTransferResult result = {EUsbTransferStatus::eError, 0, ""};
    int transferredBytes = 0;
    const int transferResult = retryWhileTimeout(
        attempts,
        [&]() {
            return libusb_bulk_transfer(
                connection.handle,
                endpointAddress,
                buffer,
                length,
                &transferredBytes,
                timeoutMs
            );
        }
    );

    result = makeTransferResult(
        classifyBulkTransferResult(transferResult),
        transferResult,
        transferredBytes,
        minimumLength
    );

    return result;
}
/*----------------------------------------------------------------------------*/

/** @fn controlWrite */
SUsbTransferResult controlWrite(
    const SUsbConnection &connection,
    const uint8_t request,
    const uint8_t *data,
    const uint16_t length,
    const uint16_t value,
    const uint16_t index,
    const unsigned int timeoutMs,
    const unsigned int attempts
) {
    SUsbTransferResult result = {EUsbTransferStatus::eError, 0, ""};
    const int transferResult = retryWhileTimeout(
        attempts,
        [&]() {
            return libusb_control_transfer(
                connection.handle,
                LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
                    LIBUSB_RECIPIENT_DEVICE,
                request,
                value,
                index,
                const_cast<uint8_t*>(data),
                length,
                timeoutMs
            );
        }
    );

    result = makeTransferResult(
        classifyControlTransferResult(transferResult),
        transferResult,
        (transferResult >= 0) ? transferResult : 0,
        static_cast<int>(length)
    );

    return result;
}
/*----------------------------------------------------------------------------*/

/** @fn controlRead */
SUsbTransferResult controlRead(
    const SUsbConnection &connection,
    const uint8_t request,
    uint8_t *buffer,
    const uint16_t length,
    const uint16_t value,
    const uint16_t index,
    const unsigned int timeoutMs,
    const unsigned int attempts,
    const uint16_t minimumLength
) {
    SUsbTransferResult result = {EUsbTransferStatus::eError, 0, ""};
    const int transferResult = retryWhileTimeout(
        attempts,
        [&]() {
            return libusb_control_transfer(
                connection.handle,
                LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
                    LIBUSB_RECIPIENT_DEVICE,
                request,
                value,
                index,
                buffer,
                length,
                timeoutMs
            );
        }
    );

    result = makeTransferResult(
        classifyControlTransferResult(transferResult),
        transferResult,
        (transferResult >= 0) ? transferResult : 0,
        static_cast<int>(minimumLength)
    );

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
        sizeof(kSupportedDevices) / sizeof(kSupportedDevices[0]);

    for (size_t index = 0U; index < deviceCount; ++index) {
        if (
            (kSupportedDevices[index].vendorId == vendorId) &&
            (kSupportedDevices[index].productId == productId)
        ) {
            result = &kSupportedDevices[index];
            break;
        }
    }

    return result;
}
/*----------------------------------------------------------------------------*/

/** @fn findDevice */
static libusb_device* findDevice(
    libusb_device **deviceList,
    const ssize_t deviceCount,
    const uint16_t vendorId,
    const uint16_t productId,
    const bool matchLocation,
    const uint8_t busNumber,
    const uint8_t deviceAddress
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

        if (
            (descriptorResult == LIBUSB_SUCCESS) &&
            (descriptor.idVendor == vendorId) &&
            (descriptor.idProduct == productId) &&
            (!matchLocation ||
             ((libusb_get_bus_number(candidate) == busNumber) &&
              (libusb_get_device_address(candidate) == deviceAddress)))
        ) {
            result = candidate;
            break;
        }
    }

    return result;
}
/*----------------------------------------------------------------------------*/

/** @fn resolveFirmwarePaths */
static SFirmwarePaths resolveFirmwarePaths(
    const SSupportedDevice &supportedDevice
) {
    static const char* const candidateDirs[] = {
        "firmware", "../firmware", "../../firmware"
    };
    SFirmwarePaths paths;
    const char *firmwareDir = getenv("OSCILLOSCOPE_FIRMWARE_DIR");
    const size_t candidateCount =
        sizeof(candidateDirs) / sizeof(candidateDirs[0]);
    size_t index;

    if (firmwareDir == NULL) {
        firmwareDir = candidateDirs[0];

        for (index = 0U; index < candidateCount; ++index) {
            const std::string candidateLoaderPath =
                std::string(candidateDirs[index]) + "/" +
                supportedDevice.firmwareBaseName + "_loader.hex";

            if (fileExists(candidateLoaderPath)) {
                firmwareDir = candidateDirs[index];
                break;
            }
        }
    }

    paths.loaderHexPath =
        std::string(firmwareDir) + "/" +
        supportedDevice.firmwareBaseName + "_loader.hex";
    paths.firmwareHexPath =
        std::string(firmwareDir) + "/" +
        supportedDevice.firmwareBaseName + "_firmware.hex";

    return paths;
}
/*----------------------------------------------------------------------------*/

/** @fn classifyBulkTransferResult */
static EUsbTransferStatus classifyBulkTransferResult(const int transferResult) {
    EUsbTransferStatus result = EUsbTransferStatus::eError;

    if (transferResult == LIBUSB_SUCCESS) {
        result = EUsbTransferStatus::eSuccess;
    }
    else if (transferResult == LIBUSB_ERROR_TIMEOUT) {
        result = EUsbTransferStatus::eTimeout;
    }
    else if (transferResult == LIBUSB_ERROR_NO_DEVICE) {
        result = EUsbTransferStatus::eNoDevice;
    }

    return result;
}
/*----------------------------------------------------------------------------*/

/** @fn classifyControlTransferResult */
static EUsbTransferStatus classifyControlTransferResult(
    const int transferResult
) {
    EUsbTransferStatus result = EUsbTransferStatus::eError;

    if (transferResult >= 0) {
        result = EUsbTransferStatus::eSuccess;
    }
    else if (transferResult == LIBUSB_ERROR_TIMEOUT) {
        result = EUsbTransferStatus::eTimeout;
    }
    else if (transferResult == LIBUSB_ERROR_NO_DEVICE) {
        result = EUsbTransferStatus::eNoDevice;
    }

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn makeTransferResult */
static SUsbTransferResult makeTransferResult(
    const EUsbTransferStatus status,
    const int transferResult,
    const int transferredBytes,
    const int minimumLength
) {
    SUsbTransferResult result = {status, transferredBytes, ""};

    if (
        (result.status == EUsbTransferStatus::eSuccess) &&
        (result.transferredBytes < minimumLength)
    ) {
        result.status = EUsbTransferStatus::eShortTransfer;
    }

    if (result.status == EUsbTransferStatus::eShortTransfer) {
        result.errorMessage = "short transfer";
    }
    else if (result.status != EUsbTransferStatus::eSuccess) {
        result.errorMessage = libusb_error_name(transferResult);
    }

    return result;
}

} // namespace usb
} // namespace oscilloscope
/******************************************************************************/
