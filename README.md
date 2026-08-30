# Oscilloscope

New cross-platform USB oscilloscope client developed without Qt. The primary
target platforms are Linux Mint, Ubuntu, and Debian, with a future Windows
build supported through CMake.

The initial target device is the Hantek DSO-2250 USB oscilloscope. The
`OldQtCode` directory contains the historical Qt4/KDE4 implementation and is
used only as a reference for protocol details and algorithms. The new codebase
is developed independently and has no Qt dependency.

## Status

Current version: `0.2.0`.

The current iteration provides the first UI shell:

- CMake build configuration for C++14;
- `Debug` and `Release` build targets;
- a Makefile and Bash build script for Linux;
- an SDL2 window with an OpenGL 3 context;
- Dear ImGui integration, a menu, control panel, status line, and display grid;
- a pinned Dear ImGui source dependency (`v1.90.9`) fetched by CMake.

The shell provides presentation-only controls for acquisition state, demo mode,
timebase, and the two channel scales. USB acquisition and waveform processing
are planned for later iterations.

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
├── core/           Shared types and application logic.
├── docs/           Project documentation.
├── render/         Oscilloscope waveform rendering.
├── ui/             User interface.
├── usb/            USB device communication.
├── WorkingDocs/    Technical specification and device documentation.
├── OldQtCode/      Historical Qt4/KDE4 reference implementation.
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

On Debian, Ubuntu, and Linux Mint:

```bash
sudo apt-get update
sudo apt-get install build-essential cmake make pkg-config libsdl2-dev libgl1-mesa-dev
```

Dear ImGui is downloaded automatically by CMake during configuration. libusb
will be required when the USB layer is implemented.

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
python3 make_release.py 0.1.1 --skip-build
```

The script synchronizes:

- the CMake project version in `CMakeLists.txt`;
- the `@version` field in `app/main.cpp`;
- the `VERSION_MAJOR`, `VERSION_MINOR`, and `VERSION_PATCH` macros;
- the embedded `FileVersion` and `ProductVersion` strings.

Without `--skip-build`, the script builds the project after updating the
metadata and restores the previous files if the build fails.

## Next Steps

1. Add a demo waveform to the display grid.
2. Implement a two-channel model, timebase, and basic controls.
3. Add libusb support and a safe acquisition thread.
4. Handle device disconnection and fallback to demo mode.

The full goals, constraints, and architecture are documented in
`WorkingDocs/TECHNICAL_SPECIFICATION.md`.

