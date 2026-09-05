# Oscilloscope

**Version:** 0.2.6 | **Status:** active development

Modern cross-platform client for Hantek USB oscilloscopes. Linux Mint and
Ubuntu are the primary development and testing platforms; compatibility with
Windows 10 and 11 is planned through the portable CMake-based architecture.

## Overview

The initial target device is the Hantek DSO-2250 USB oscilloscope. The
application uses SDL2, Dear ImGui, and OpenGL for the user interface, and
libusb for communication with supported instruments.

## Current functionality

- SDL2 window with an OpenGL rendering context;
- Dear ImGui oscilloscope user-interface shell;
- display grid, control panel, menu bar, and status line;
- Demo and Live modes;
- supported-device enumeration through libusb;
- detection of the Hantek DSO-2250 bootloader (`0x04B4:0x2250`) and
	operational device (`0x04B5:0x2250`);
- FX2 firmware upload through `fxload` and bounded re-enumeration polling;
- USB interface claiming and clean connect/disconnect handling;
- Start/Stop-controlled endpoint polling;
- bounded buffering between USB reads and packet processing;
- capture-state polling through vendor control requests and bulk endpoints;
- bounded USB error recovery and safe device-loss handling.
- deterministic DSO-2250 capture-state and two-channel sample parsing.
- complete 32,768-sample two-channel capture reads after a successful trigger.
- thread-safe publication of the latest decoded waveform and trigger point.
- a per-device capture profile selected from the supported-device table.

## Architecture

| Module | Responsibility |
| --- | --- |
| `app/` | Application entry point, event loop, and UI composition |
| `capture/` | Sample acquisition and buffering |
| `core/` | Planned shared application types and state |
| `docs/` | Generated documentation sources |
| `firmware/` | Default path for local device firmware files |
| `render/` | Planned waveform rendering |
| `tests/` | Automated tests run through CTest |
| `tools/` | Optional firmware extraction utilities |
| `ui/` | Planned reusable interface components |
| `usb/` | Device discovery, firmware loading, and USB communication |

## USB device operation

The USB module enumerates the full libusb device list and compares every
descriptor against its central table of supported VID/PID pairs. Each match is
reported with its model name, VID/PID, bus number, and address, allowing
multiple identical instruments to be distinguished.

An uninitialized DSO-2250 appears as `04b4:2250`. The application uploads the
externally supplied FX2 firmware, waits for the instrument to re-enumerate as
`04b5:2250`, and opens its bulk endpoints on interface 0, alternate setting 0.
The official installer prohibits unauthorized redistribution, so the firmware
is excluded until separate redistribution rights can be confirmed. A repaired
extractor in `tools/dsoextractfw.c` allows owners to generate the required HEX
files from their copy of the official `Dso2250x861.sys` driver.

Pressing Connect prepares the USB device without starting acquisition. Pressing
Start launches a USB producer that sends the capture-state command through bulk
endpoint `0x02` and reads a 512-byte response from endpoint `0x86`, with the
required `B3` and `B2` vendor requests. Successful responses enter a bounded
FIFO and a processing consumer reads them independently. Stop closes the queue,
wakes the consumer, and joins both threads before releasing the connection.

See `oscilloscope::usb::enumerateSupportedDevices()` for the public discovery
API.

## Build requirements

The Linux build requires CMake 3.16 or later, a C++14 compiler, GNU Make,
SDL2, OpenGL, and libusb-1.0 development packages. Refer to `README.md` for
complete installation, build, and run instructions.

## Planned work

The next task is displaying live waveforms. Instrument controls, broader
recovery behavior, and persistent configuration follow in later milestones.

