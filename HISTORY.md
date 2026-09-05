# Oscilloscope - Project History

Milestone log for the Oscilloscope project.
Records key decisions, structural changes, and completed development stages.

---

## 2026-09-05

### Stage 4 preparation - Release metadata coverage

- Registered standalone C++ test sources in dedicated CMake source lists.
- Extended the release updater to discover C++ test sources and update their
  `@version` metadata together with production sources.
- Added deterministic CTest coverage that runs the updater in an isolated copy
  of the project and verifies test-source version updates.
- Corrected the documented DSO-2250 operational endpoint configuration to
  interface 0, alternate setting 0.

### Stage 4 - Sample parser foundation

- Added pure DSO-2250 parser functions for capture-state responses and complete
  interleaved two-channel waveform buffers.
- Preserved the legacy byte layout: each sample pair is CH2 followed by CH1.
- Preserved the legacy capture-state trigger-point transformation in the new,
  Qt-independent parser.
- Added deterministic CTest coverage for state parsing, trigger decoding,
  channel order, and malformed capture buffers.

### Stage 4 - Complete DSO-2250 capture reads

- Start acquisition with the legacy capture-start and trigger-enable command
  sequence before capture-state polling begins.
- On a complete DSO-2250 capture-state response, read the fixed two-channel
  32768-sample buffer as 128 consecutive 512-byte bulk packets.
- Stop incomplete waveform publication at the first failed packet and preserve
  the existing bounded recovery and device-loss behavior.
- Restart capture and re-enable the trigger only after a complete buffer was
  read and queued for processing.
- Added acquisition-status diagnostics for channel-data, capture-start, and
  trigger-enable failures.

### Stage 4 - Decoded waveform publication

- Preserved the decoded capture trigger point alongside its raw waveform bytes
  while the frame passes through the FIFO.
- Decode complete queued DSO-2250 frames in the processing worker instead of
  interpreting raw channel bytes as capture-state data.
- Added a mutex-protected latest-waveform snapshot and `getLatestWaveform()`
  API for the future renderer.
- Extended FIFO tests to verify trigger-point metadata is retained with each
  packet.

### Capture protocol profiles

- Moved capture endpoints, command bytes, packet length, channel layout,
  sample count, and completion state into each supported-device entry.
- Preserve the selected profile in the active USB connection and use it for
  acquisition and waveform parsing instead of model-named function contracts.
- Validate profile packetisation and fixed capture-storage bounds before
  requesting sample data.

### Stage 4 - Hardware verification

- Verified a 60-second acquisition on the initial Hantek DSO-2250 profile.
- Verified six 10-second Start/Stop cycles: the UI stayed responsive and the
  connected device was ready for every subsequent Start.
- Verified active USB removal: acquisition stopped without a crash, deadlock,
  or unbounded wait and reported the lost device while beginning a command.
- Verified automatic rediscovery, explicit reconnection, and a subsequent
  successful acquisition after the device was reattached.
- Verified application shutdown during acquisition without a crash, deadlock,
  or unbounded wait; the instrument returned to its connected idle LED state,
  then turned off after application exit.
- Verified the Release build without warnings. Normal acquisition showed no
  persistent recovery state or unexpected acquisition stop.

### Code style - Immutable object naming

- Reserved `UPPER_CASE` for preprocessor macros.
- Renamed immutable module objects to the `kPascalCase` convention while
  retaining `const` storage and unchanged runtime behavior.

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
- Implemented device opening, USB interface claiming, interface release, and
  handle/context cleanup for the selected Hantek DSO-2250.
- Added Connect and Disconnect controls with connection state in the status
  line.
- Disconnect automatically when entering Demo mode, on device loss after a
  rescan, and during application shutdown.
- Added a Linux udev rule for non-root DSO-2250 access through libusb.
- Verified the connect/disconnect lifecycle with a connected Hantek DSO-2250.

## 2026-09-02

### Research - FX2 firmware upload requirement

- Investigated the old code for a host "presence ping" that would explain the
  DSO-2250 status LED behavior (red blink on USB link, green blink on host
  activity, long red+green during data bursts).
- Found no dedicated ping command: `HantekDSOAThread::run()` in
  `OldQtCode/src/hantekdsoathread.cpp` polls `dsoGetCaptureState` in a loop
  with a `msleep(timeBase)` delay; the repeated bulk transaction itself is
  what the firmware reports as host activity.
- Found that the DSO-2250 uses a Cypress EZ-USB FX2 chip with RAM-resident
  firmware. `OldQtCode/dsoextractfw/HantekDSO.rules` uploads
  `DSO2250_firmware.hex` and `DSO2250_loader.hex` through `fxload` on every
  USB "add" event; `OldQtCode/dsoextractfw/dsoextractfw.c` extracts those hex
  files from the official Windows driver (`1.SYS`).
- Until firmware is uploaded, the device stays in a bare bootloader state:
  no status LED activity and no working bulk endpoints. This explains the
  LED blinking seen on Windows (official driver uploads firmware
  automatically) versus no blinking on Linux with the current codebase (no
  firmware upload step exists yet, and no `.hex` firmware files are present
  in this repository).
- The firmware `.hex` files are not included because the official installer
  prohibits unauthorized redistribution; they must be extracted from a
  user's copy of the official Windows driver before the USB layer can work
  end-to-end.

## 2026-09-04

### Stage 3 - Endpoint read loop complete

- Corrected the extracted DSO-2250 loader and firmware images and verified
  their Intel HEX checksums; the files remain excluded from Git because
  redistribution rights have not been established.
- Added support for the operational `04b5:2250` identity alongside the
  `04b4:2250` bootloader identity.
- Wait for FX2 re-enumeration with bounded polling after firmware upload and
  reconnect to the operational device on interface 0, alternate setting 0.
- Extended the supplied udev rule to grant access to both USB identities.
- Added vendor control-transfer helpers and the DSO-2250 initialization
  sequence required before endpoint polling.
- Implemented a dedicated acquisition thread that sends the capture-state
  request to bulk endpoint `0x02` and reads 512-byte responses from endpoint
  `0x86`.
- Added successful-poll, transfer-error, and last-capture-state tracking with
  thread-safe counters.
- Made endpoint activity follow the Start/Stop lifecycle instead of beginning
  at Connect; Stop, Disconnect, mode changes, application shutdown, and device
  loss stop and join the acquisition thread before releasing USB resources.
- Disabled Start until a live device is connected and disabled device rescans
  while a connection is active.
- Select Live mode at startup when a supported device is present, otherwise
  retain Demo mode.

### Hardware verification

- Verified firmware re-enumeration from `04b4:2250` to `04b5:2250` on a
  physical Hantek DSO-2250.
- Verified successful `B3`, `B2`, bulk OUT, and 512-byte bulk IN transfers.
- Confirmed the instrument LED lifecycle: red after Connect, green during
  Start/acquisition, red after Stop, and off after Disconnect.

### Firmware extractor repair

- Added a maintained extraction utility under `tools/` for users who possess
  the official `Dso2250x861.sys` Windows driver.
- Fixed the historical extractor's unhandled padding byte, which inserted an
  extra zero and dropped the final data byte in every Intel HEX record.
- Skip empty 22-byte separator records instead of emitting invalid
  `:0000000000` lines.
- Fixed loader range calculation and added validation for record layout, EOF
  records, and generated checksums.
- Verified extraction from the official `Dso2250x861.sys` driver end to end;
  both generated HEX files match the hardware-tested files byte for byte.

### USB module structure

- Moved USB headers to `usb/inc/` and implementations to `usb/src/` after the
  module grew beyond a single source/header pair.
- Updated CMake source lists, include directories, and dependent includes for
  the new layout.

### Stage 3 - USB timeout and error recovery

- Reject short USB writes and responses that do not contain the minimum data
  required by the protocol operation.
- Stop each capture-state transaction at its first failed transfer instead of
  issuing the remaining USB operations with invalid state.
- Retry transient timeouts and I/O errors with bounded attempts and delays.
- Stop acquisition immediately when libusb reports that the device was lost.
- Join an acquisition worker before releasing a lost device's USB resources.
- Keep a connected device available for another Start attempt after repeated
  recoverable transfer errors.
- Report recovery, terminal I/O errors, and device loss through application
  states in the status line.
- Removed the temporary poll/error counters and per-cycle stderr diagnostics.
- Verified on physical hardware that unplugging the device during acquisition
  stops acquisition and closes the connection.
- Added periodic presence checks outside acquisition so an unplugged connected
  device is detected and a returned device appears without a manual rescan.
- Removed the redundant manual rescan control and kept the device-disconnected
  status visible until the device returns.
- Verified on physical hardware that idle disconnection is detected, the
  disconnected status remains visible, and active acquisition behavior is
  unchanged.

### Code review and cleanup

- Reviewed the non-Qt codebase against the project style rules, DRY, and KISS.
- Merged the duplicate device-matching helpers into a single `findDevice`.
- Extracted a `retryWhileTimeout` helper shared by the bulk and control
  transfer wrappers.
- Shared a single `fileExists` helper between the USB and firmware modules.
- Factored acquisition-fault handling and USB presence polling out of the
  main loop into dedicated helpers.
- Added full Doxygen descriptions to the application entry-point functions.
- Enabled `-Wall -Wextra -Wpedantic` for GCC/Clang; the build is warning-free
  and behavior is unchanged.
- Noted the sole visible change: before firmware upload the status line now
  shows the enumerated bootloader's model name, which correctly reflects the
  unprogrammed device.

### Stage 3 - Raw USB buffer management

- Added a fixed-capacity FIFO between USB reads and packet processing.
- Store each raw response with its actual transferred length and support the
  largest known two-channel DSO-2250 capture without allocation in the active
  acquisition loop.
- Keep the USB producer non-blocking by discarding the oldest queued response
  when processing falls behind, with a thread-safe dropped-packet count.
- Added a processing consumer that preserves the existing capture-state update
  while decoupling it from the USB polling thread.
- Close the queue and wake the consumer during Stop and terminal USB errors;
  join both workers before releasing USB resources.
- Added deterministic CTest coverage for FIFO order, response length, overflow,
  close/reset behavior, and concurrent producer-consumer operation.
- Moved the expanded capture module into `capture/inc/` and `capture/src/`.

### Hardware verification

- Verified a 60-second acquisition run and 11 repeated Start/Stop cycles on a
  physical Hantek DSO-2250 without GUI stalls or loss of the USB connection.
- Confirmed the expected LED lifecycle: red while connected and stopped, green
  during acquisition, and off after application shutdown.
- Verified active USB removal: acquisition stopped without a hang and reported
  `USB device lost while beginning command`.
- Verified automatic device rediscovery, explicit reconnection, and successful
  acquisition after reconnecting the instrument.
- Verified application shutdown during acquisition without a crash, deadlock,
  or unbounded wait.
- No persistent recovery state or unexpected acquisition stop occurred during
  normal operation; the Release build and FIFO test passed without warnings.

### Next USB tasks

- Decode capture-state responses and acquired sample packets.
- Add deterministic fault-injection tests for timeout and transfer-error
  recovery, then verify recovery with a connected physical device.
