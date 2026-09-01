/**
 * @file    usb_device.h
 * @version 0.2.0
 * @authors Anton Chernov
 * @date    2026-09-01
 */

#ifndef USB_DEVICE_H_
#define USB_DEVICE_H_

/******************************* Included files ******************************/
#include <stdint.h>
#include <string>
#include <vector>

/********************************* Definitions ********************************/

namespace oscilloscope {
namespace usb {

enum class EScanStatus {
    eSuccess,
    eInitializationFailed,
    eEnumerationFailed
};

struct SUsbDeviceInfo {
    uint16_t vendorId;
    uint16_t productId;
    uint8_t busNumber;
    uint8_t deviceAddress;
    const char* modelName;
};

struct SUsbScanResult {
    EScanStatus status;
    std::vector<SUsbDeviceInfo> devices;
    std::string errorMessage;
};

/********************* Application Programming Interface *********************/

SUsbScanResult enumerateSupportedDevices();

} // namespace usb
} // namespace oscilloscope

#endif // USB_DEVICE_H_
/******************************************************************************/
