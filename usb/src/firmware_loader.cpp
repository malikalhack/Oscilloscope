/**
 * @file    firmware_loader.cpp
 * @version 0.2.2
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

/******************************* Included files *******************************/
#include <sys/stat.h>
#include <sys/wait.h>
#include <cstdio>
#include <unistd.h>

#include "firmware_loader.h"

/********************************* Definitions ********************************/

namespace oscilloscope {
namespace usb {

/***************************** Private prototypes *****************************/

/**
 * @brief Checks whether a regular file exists at the given path
 * @param[in] path File path to check
 * @returns True when the file exists
 */
static bool fileExists(const std::string &path);

/********************* Application Programming Interface **********************/

/** @fn loadFx2Firmware */
SFirmwareLoadResult loadFx2Firmware(
    const uint8_t busNumber,
    const uint8_t deviceAddress,
    const SFirmwarePaths &paths
) {
    SFirmwareLoadResult result = {EFirmwareLoadStatus::eLoaded, ""};
    char devicePath[32];
    pid_t childPid = -1;
    int childStatus = 0;

    if (!fileExists(paths.firmwareHexPath) || !fileExists(paths.loaderHexPath)) {
        result.status = EFirmwareLoadStatus::eHexFileNotFound;
        result.errorMessage =
            "Firmware or loader .hex file not found: " +
            paths.firmwareHexPath + ", " + paths.loaderHexPath;
    }
    else {
        std::snprintf(
            devicePath,
            sizeof(devicePath),
            "/dev/bus/usb/%03u/%03u",
            busNumber,
            deviceAddress
        );

        childPid = fork();

        if (childPid < 0) {
            result.status = EFirmwareLoadStatus::eUploadFailed;
            result.errorMessage = "fork() failed while launching fxload";
        }
        else if (childPid == 0) {
            execlp(
                "fxload", "fxload",
                "-t", "fx2",
                "-I", paths.firmwareHexPath.c_str(),
                "-s", paths.loaderHexPath.c_str(),
                "-D", devicePath,
                (char*)NULL
            );
            /* Fall back to the common sbin location: GUI launchers often
             * run with a PATH that omits /usr/sbin. */
            execl(
                "/usr/sbin/fxload", "fxload",
                "-t", "fx2",
                "-I", paths.firmwareHexPath.c_str(),
                "-s", paths.loaderHexPath.c_str(),
                "-D", devicePath,
                (char*)NULL
            );
            _exit(127); /* both exec attempts failed */
        }
        else if (waitpid(childPid, &childStatus, 0) < 0) {
            result.status = EFirmwareLoadStatus::eUploadFailed;
            result.errorMessage = "waitpid() failed while running fxload";
        }
        else if (!WIFEXITED(childStatus) || (WEXITSTATUS(childStatus) == 127)) {
            result.status = EFirmwareLoadStatus::eFxloadNotFound;
            result.errorMessage = "fxload utility was not found in PATH";
        }
        else if (WEXITSTATUS(childStatus) != 0) {
            result.status = EFirmwareLoadStatus::eUploadFailed;
            result.errorMessage = "fxload exited with a non-zero status";
        }
    }

    return result;
}

/****************************** Private functions *****************************/

/** @fn fileExists */
static bool fileExists(const std::string &path) {
    struct stat statBuffer;
    return stat(path.c_str(), &statBuffer) == 0;
}

} // namespace usb
} // namespace oscilloscope
/******************************************************************************/
