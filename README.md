# Oscilloscope

New cross-platform USB oscilloscope client developed without Qt. The primary
target platforms are Linux Mint, Ubuntu, and Debian, with a future Windows
build supported through CMake.

The initial target device is the Hantek DSO-2250 USB oscilloscope. The
`OldQtCode` directory contains the historical Qt4/KDE4 implementation and is
used only as a reference for protocol details and algorithms. The new codebase
is developed independently and has no Qt dependency.

## Status

Current version: `0.1.0`.

The first iteration provides a minimal application skeleton:

- CMake build configuration for C++14;
- `Debug` and `Release` build targets;
- a Makefile and Bash build script for Linux;
- the `run` executable;
- synchronized version fields in `CMakeLists.txt` and `app/main.cpp`;
- a console smoke test that prints `Hello world!`.

SDL2, Dear ImGui, OpenGL, and libusb are not connected at this stage. The
oscilloscope window, USB acquisition, and demo mode are planned for later
iterations.

## Planned Stack

- C++14;
- CMake and Make;
- SDL2;
- Dear ImGui;
- OpenGL;
- libusb-1.0.

## Directory Layout

```text
app/         Application entry point.
capture/     Sample acquisition and processing.
core/        Shared types and application logic.
render/      Oscilloscope waveform rendering.
ui/          User interface.
usb/         USB device communication.
OldQtCode/   Historical Qt4/KDE4 source for reference only.
WorkingDocs/ Technical specification and device documentation.
```

## First-Iteration Requirements

The current application skeleton requires:

- CMake 3.16 or newer;
- a compiler with C++14 support, such as GCC or Clang;
- GNU Make for builds through `make`.

On Debian, Ubuntu, and Linux Mint:

```bash
sudo apt update
sudo apt install build-essential cmake make
```

SDL2, OpenGL, and libusb will be required once the graphical interface and USB
layer are implemented.

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

1. Create an SDL2 window and integrate Dear ImGui.
2. Add an oscilloscope grid and a demo waveform.
3. Implement a two-channel model, timebase, and basic controls.
4. Add libusb support and a safe acquisition thread.
5. Handle device disconnection and fallback to demo mode.

The full goals, constraints, and architecture are documented in
`WorkingDocs/TECHNICAL_SPECIFICATION.md`.