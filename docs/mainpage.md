# Oscilloscope

**Version:** 0.2.1 | **Status:** active development

Cross-platform USB oscilloscope client for Linux Mint, Ubuntu, and Debian.
The project is written from scratch in C++14 and does not depend on Qt.

## Overview

The initial target device is the Hantek DSO-2250 USB oscilloscope. The
application uses SDL2, Dear ImGui, and OpenGL for the user interface, and
libusb for communication with supported instruments.

The `OldQtCode/` directory is retained solely as a historical reference for
USB protocol research and signal-processing algorithms. It is not a dependency
of the new implementation.

## Current functionality

- SDL2 window with an OpenGL rendering context;
- Dear ImGui oscilloscope user-interface shell;
- display grid, control panel, menu bar, and status line;
- Demo and Live modes;
- supported-device enumeration through libusb;
- detection of all connected Hantek DSO-2250 instruments (`0x04B4:0x2250`);
- manual USB rescan and automatic scan when entering Live mode.

## Architecture

| Module | Responsibility |
| --- | --- |
| `app/` | Application entry point, event loop, and UI composition |
| `core/` | Shared application types and state |
| `usb/` | Device discovery and USB communication |
| `capture/` | Sample acquisition and buffering |
| `render/` | Waveform rendering |
| `ui/` | Reusable interface components |

## USB device discovery

The USB module enumerates the full libusb device list and compares every
descriptor against its central table of supported VID/PID pairs. Each match is
reported with its model name, VID/PID, bus number, and address, allowing
multiple identical instruments to be distinguished.

See `oscilloscope::usb::enumerateSupportedDevices()` for the public discovery
API.

## Build requirements

The Linux build requires CMake 3.16 or later, a C++14 compiler, GNU Make,
SDL2, OpenGL, and libusb-1.0 development packages. Refer to `README.md` for
complete installation, build, and run instructions.

## Planned work

The next USB tasks are device connection, interface claiming, endpoint reads,
buffering, and recovery from I/O errors or disconnects. Waveform processing,
instrument controls, and persistent configuration follow in later milestones.

