# Oscilloscope - Project History

Milestone log for the Oscilloscope project.
Records key decisions, structural changes, and completed development stages.

---

## 2026-08-27

### Project inception

- Created a new cross-platform USB oscilloscope client without Qt.
- Selected C++14, CMake, Make, SDL2, Dear ImGui, OpenGL, and libusb as the
  planned technology stack.
- Selected the Hantek DSO-2250 USB oscilloscope as the initial target device.
- Kept `OldQtCode/` as a read-only historical reference for USB protocol
  research and algorithms; the new implementation remains independent.

### Repository setup

- Created the Git repository.
- Added the initial project documentation and ignore rules.

## 2026-08-28

### Stage 1 - Project Initialization complete

- Prepared the new-code directory structure: `app/`, `core/`, `usb/`,
  `capture/`, `render/`, `ui/`, and `docs/`.
- Configured the root CMake build for C++14.
- Added Make-based parallel build entry points and Linux build/clean helper
  scripts.
- Added Debug and Release build configurations.
- Built and verified the initial hello-world application on Linux.

### Documentation

- Updated `README.md` with project status, Linux dependencies, and build/run
  instructions.

## 2026-08-31

### Stage 2 - Basic UI Shell

- Added SDL2 and OpenGL dependencies to the CMake build.
- Added Dear ImGui v1.90.9 through CMake FetchContent.
- Created the initial SDL2 window with an OpenGL 3.2 core context.
- Integrated the Dear ImGui SDL2 and OpenGL3 backends into the event and render
  loop.
- Implemented the initial oscilloscope shell: menu bar, control panel, display
  area, status line, and oscilloscope grid.
- Added presentation-only controls for acquisition state, demo mode, timebase,
  channel visibility, and voltage scale.
- Updated `README.md` to document the completed UI-shell iteration.

### Continuous integration

- Added repository CI configuration.

## 2026-09-01

### UI layout refinement

- Moved the oscilloscope control panel to the right of the waveform display,
  matching the physical instrument layout.
- Fixed status-line clipping by placing it directly beneath the waveform area.
- Updated `.gitignore` with minor repository housekeeping changes.
- Verified the Release build after the UI layout changes.

### Stage 3 - USB detection and connection

- Added `libusb-1.0` as a required CMake dependency.
- Implemented supported-device enumeration through libusb.
- Added the Hantek DSO-2250 device signature: VID `0x04B4`, PID `0x2250`.
- Scan every connected USB device and retain all supported-device matches with
  their bus number, address, VID/PID, and model name.
- Added a `Rescan devices` control and status-line reporting for detected
  devices and libusb errors.
- Rescan automatically when switching from Demo mode to Live mode.
- Verified detection with a connected Hantek DSO-2250 and a successful Release
  build.

### Next USB tasks

- Connect to the selected device and claim its USB interface.
- Read endpoint data chunks in a dedicated acquisition path.
- Add buffering between USB reads and waveform processing.
- Handle timeouts, I/O errors, disconnects, and recovery states.
