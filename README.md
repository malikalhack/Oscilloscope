# Oscilloscope

Modern cross-platform client for Hantek USB oscilloscopes. Linux Mint and
Ubuntu are the primary development and testing platforms; compatibility with
Windows 10 and 11 is planned through the portable CMake-based architecture.

## Status

Current version: `0.2.6`.

The application currently provides:

- an SDL2, Dear ImGui, and OpenGL user interface;
- Demo and Live operating modes;
- supported Hantek device discovery and connection through libusb;
- FX2 firmware upload and operational-device re-enumeration;
- Start/Stop-controlled endpoint polling;
- bounded USB error recovery and safe device-loss handling.

Capture-state responses and sample buffers are decoded by deterministic,
hardware-independent parser functions. Each supported-device entry supplies its
capture protocol, including endpoints, packet size, commands, channel layout,
sample count, and completion state. After a completed capture, the acquisition
worker reads and queues the complete profile-defined sample buffer, then starts
the next capture. Live waveform rendering is not yet implemented. A processing
worker decodes complete captures and publishes the latest frame and trigger
point safely for rendering.

## Planned Stack

- C++14;
- CMake and Make;
- SDL2;
- Dear ImGui;
- OpenGL;
- libusb-1.0.

## Directory Layout

```text
Oscilloscope/
├── app/            Application entry point.
├── capture/        Sample acquisition and processing.
│   ├── inc/        Capture module headers.
│   └── src/        Capture module implementations.
├── core/           Shared types and application logic.
├── docs/           Project documentation.
├── firmware/       Default path for local device firmware files.
├── render/         Oscilloscope waveform rendering.
├── tests/          Automated tests run through CTest.
├── tools/          Optional firmware extraction utilities.
├── ui/             User interface.
├── usb/            USB device communication.
│   ├── inc/        USB module headers.
│   └── src/        USB module implementations.
├── WorkingDocs/    Technical specification and device documentation.
├── CMakeLists.txt  CMake build configuration.
├── Makefile        Make build entry points.
└── linux_build.sh  Linux build script.
```

## Requirements

The current application requires:

- CMake 3.16 or newer;
- a compiler with C++14 support, such as GCC or Clang;
- GNU Make for builds through `make`.
- SDL2 development files;
- OpenGL development files.
- libusb-1.0 development files.
- `fxload` for uploading the RAM-resident FX2 firmware.
- binutils development files for building the optional firmware extractor.

On Debian, Ubuntu, and Linux Mint:

```bash
sudo apt-get update
sudo apt-get install build-essential cmake make pkg-config libsdl2-dev \
	libgl1-mesa-dev libusb-1.0-0-dev fxload binutils-dev
```

Dear ImGui is downloaded automatically by CMake during configuration.

### Hantek DSO-2250 USB Access

Linux requires a udev rule for a regular desktop user to open the Hantek device
through libusb. The rule covers both the `04b4:2250` bootloader identity and the
`04b5:2250` operational identity. Install the supplied rule, reload udev rules,
then reconnect the oscilloscope:

```bash
sudo cp usb/80-hantek-dso-2250.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### Hantek DSO-2250 Firmware

The official installer states that unauthorized reproduction or distribution
of the program, or any portion of it, is prohibited. Because separate firmware
redistribution rights have not been established, the extracted firmware is not
stored in this repository. Supply the files at these default paths:

```text
firmware/DSO2250_loader.hex
firmware/DSO2250_firmware.hex
```

Set `OSCILLOSCOPE_FIRMWARE_DIR` to use another directory. On Connect, the
application uploads the firmware to a `04b4:2250` device with `fxload`, waits
for it to re-enumerate as `04b5:2250`, and then opens the operational device.

The repository provides a repaired extractor for users who have the official
32-bit Windows driver. Locate `Dso2250x861.sys` in the extracted driver package,
then run:

```bash
mkdir -p firmware Output
cc -std=c11 -Wall -Wextra -Wpedantic tools/dsoextractfw.c \
	-o Output/dsoextractfw -lbfd
./Output/dsoextractfw /path/to/Dso2250x861.sys firmware
```

The utility validates record sizes, Intel HEX record types, and EOF records,
and calculates checksums while writing `DSO2250_firmware.hex` and
`DSO2250_loader.hex`.

## Build

### Makefile

Release build:

```bash
make release
```

Debug build:

```bash
make debug
```

Clean the build-output directory:

```bash
make clean
```

### Bash Script

Release build:

```bash
./linux_build.sh
```

Debug build:

```bash
./linux_build.sh -d
```

Clean the output directory before building:

```bash
./linux_build.sh -c
```

## Run

After a Makefile build, run the corresponding executable:

```bash
./Output/release/run
```

For the Debug configuration:

```bash
./Output/debug/run
```

By default, `linux_build.sh` uses the `Output` directory. Its executable is:

```bash
./Output/run
```

## Version Update

Versions use the `MAJOR.MINOR.PATCH` format and are updated with:

```bash
python3 make_release.py 0.1.1
```

The script synchronizes:

- the CMake project version in `CMakeLists.txt`;
- the `@version` field in `app/main.cpp`;
- the `@version` field in C++ test sources registered in CMake source lists;
- the `VERSION_MAJOR`, `VERSION_MINOR`, and `VERSION_PATCH` macros;
- the embedded `FileVersion` and `ProductVersion` strings;
- the version shown in `README.md`, `docs/mainpage.md`, and `docs/Doxyfile`.

Without `--skip-build`, the script builds the project after updating metadata
and restores the previous files if the build fails.

CI can update only the version metadata by passing `--skip-build`.


## Next Steps

1. Render live and demo waveforms on the display grid.
2. Implement the two-channel model, timebase, and instrument controls.

The full goals, constraints, and architecture are documented in
`WorkingDocs/TECHNICAL_SPECIFICATION.md`.

