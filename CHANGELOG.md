# Changelog

**English** | [中文](CHANGELOG.zh-CN.md)

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.10-rc.1] - 2026-10-08

GitHub prerelease rc.1; SDK and Python package versions remain `1.0.10`.
The 28 bundled wheels are reused unchanged from the current SDK snapshot.

### Added

- Add `setChargingSitDown()` to C++ and Python, including Python type hints and
  `MotionState.ChargingSitDown = 24` for motion-state feedback.
- Add `charging-sit` to both C++/Python server/client examples and show current
  and previous motion states in status queries and telemetry.
- Add loopback coverage for Python command encoding, state feedback, and all
  C++/Python server/client combinations.
- Add `setPoseTracking()` and `setPose(x, y, z, r)` to C++ and Python for
  BPX/BPX-Pro pose tracking, including Python type hints. Pose inputs are finite
  normalized values in `[-1, 1]`; `r` is yaw, and the two unused wire channels
  remain zero.
- Add C++ and Python pose-tracking examples using the shared connection options.
  Each example automatically stands up, waits for motion and gait feedback,
  sweeps `x`, `y`, `z`, and `r` in order, then lies down and waits for completion.
  Each axis holds `-1000` for one second, sweeps to `1000` over three seconds,
  holds for one second, then returns to zero for one second. Output includes
  all four values.
- Add loopback coverage for pose input validation, wire-channel mapping,
  gait transitions, and example sequencing and timing.
- Add `getHardEstop()` / `getHardEstopValue()` to C++ state queries and
  `getHardEstop()` to all Python state classes (with type hints): 1 while the
  hard estop switch is pressed (all joints unpowered), 0 when released, and
  `None`/false before 1 Hz feedback arrives. Carried on bit 2 of the 1 Hz
  status flags; requires robot firmware that publishes it.
- Add `getFaultState()` / `getFaultStateValue()` to C++ state queries and
  `getFaultState()` to all Python state classes (with `FaultEntry` /
  `FaultState` type hints): the full active-fault snapshot from a dedicated
  1 Hz UDP channel (head `0x00F0`), up to 32 `{code, occurred_t_ms}` entries
  plus a `truncated` flag. Entries are sorted severity-descending, so
  `entries[0]` is the top-priority fault; `entry_count=0` snapshots arrive as
  a heartbeat while the robot is fault-free. `None`/false only means no fault
  packet has been received yet (e.g. an older robot). Custom fields stay on
  the robot's local IPC channel.
- Add `getFaultTimestamp()` / `getFaultTimestampValue()` for the receive
  time of the latest fault snapshot (monotonic clock, not comparable with
  `occurred_t_ms`, which is Unix epoch milliseconds).
- Add the public header `fault_code.h` documenting the 64-bit fault-code
  bitfields (version / separator / severity / module / sub-module / sub-code)
  with `DecodeFault*()` helpers and `FaultSeverity` / `FaultModule` enums, so
  applications can render codes without internal headers.
- Show an active-fault summary (`faults=<count>[+truncated] top=0x…`) in the
  C++ and Python feedback examples.

### Changed

- Refresh the bundled native SDK headers and libraries to 1.0.10.
- Refresh the public header and bundled Linux x86_64/aarch64, Windows x86_64,
  and macOS arm64 SDK libraries for pose tracking.
- Document pose input limits, gait feedback requirements, and zeroing behavior
  when leaving pose tracking or disconnecting.
- Unknown UDP packet heads are now logged once per head value instead of once
  per packet, keeping logs clean as new channels appear.
- `FaultCodeManager` on the robot is now thread-safe and its snapshot ordering
  is deterministic (severity descending, ties by smallest packed code), which
  also makes the IPC `primary_code` stable across runs.

### Compatibility

- Requires robot firmware that publishes the fault channel. Older robots keep
  working; the fault getters just stay `None`/false.
- Older SDK versions paired with new robot firmware log one
  `unknown upload head` line per second on stderr; upgrading the SDK removes
  the noise.

## [1.0.9] - 2026-09-08

### Added

- Add interoperable C++/Python server/client examples with five jump commands, robot status queries, and timestamped battery/charger feedback.

- Add upward, forward, backward, leftward, and rightward jump APIs for BPX.
- Add robot serial-number and model queries, along with the `RobotModel` enum.
- Add current control-mode queries and the `ControlMode` enum.
- Add Python charger-insertion state getters and display them in C++ and Python examples.
- Add Python bindings and type hints for these APIs.
- Synchronously query and print the serial number and model during connection
  setup, and query them again on automatic reconnection.

### Changed

- Retry temporary network failures while polling build status and job details, retaining the wait timeout and run-resume instructions.
- Build Linux x86_64 and aarch64 wheels in parallel on native runners; update progress and artifact collection for four jobs.
- Update the bundled Linux, Windows, and macOS SDK libraries and headers to 1.0.9.
- Refresh wheel build progress in place in supported terminals and highlight
  success, failure, and cancellation results; retain successive progress entries
  when output is redirected to a log.
- Update the English and Chinese API documentation and usage examples.

## [1.0.8] - 2026-07-24

### Added

- Add resilient background TCP reconnection with bounded backoff and live
  connection-state reporting through `isConnected()` in the C++ and Python
  APIs.
- Add automatic robot time synchronization after every confirmed connection
  and reconnection. The robot applies SDK time to Linux `CLOCK_REALTIME` and
  the hardware RTC only when the absolute offset exceeds ten minutes.
- Add configurable ordinary robot-state upload rates from 1 to 200 Hz while
  preserving the native rate cap of each state group.
- Add hardware-oriented tests for robot-state upload rates and TCP reconnection
  monitoring in C++ and Python.

### Changed

- Make `connect()` start a desired background connection instead of
  guaranteeing an immediately live TCP session; callers should wait for
  `isConnected()` before issuing control commands.
- Start every new motion-control session with a complete zero command and
  require a new application command before a non-zero target can resume.
- Prevent joint control from replaying the command from a disconnected session;
  a new complete joint frame or zero command is required after reconnection.
- Send a best-effort motion command with zero target values when motion control
  is explicitly disconnected.
- Update the bundled Linux x86_64 and aarch64 SDK libraries and release wheels
  to version 1.0.8 for CPython 3.8 through 3.14.
- Update the bundled Windows x86_64 SDK DLL and import library, and add
  version 1.0.8 Windows AMD64 wheels for CPython 3.8 through 3.14.
- Update the bundled macOS arm64 SDK library, and add version 1.0.8 macOS 11+ 
  arm64 wheels for CPython 3.8 through 3.14.
- Limit the motion-level Python example's yaw command to 0.2 rad/s and extend
  its demonstration interval to 30 seconds.

### Fixed

- Ensure `setZeroPositionsFlag()` never generates the reserved zero tag,
  preventing zero-position preparation requests from being silently ignored
  by the robot.

### Removed

- Remove the explicit C++ and Python robot time-synchronization APIs and their
  examples. Applications only need to call `connect()` because synchronization
  is performed automatically.
- Remove the checked-in 1.0.7 wheel set in favor of the 1.0.8 Linux wheels.

## [1.0.7] - 2026-07-22

### Added

- Add one-command Linux and Windows wheel build scripts that prepare reusable
  Python 3.11 environments with `uv` and run `cibuildwheel`.
- Add a Windows build option that temporarily bypasses environment and system
  proxy settings.
- Add an opt-in `BPX_SDK_BUILD_HARDWARE_TESTS` CMake option and an interactive
  real-robot test for verifying that the Bound gait remains selected across a
  forward, pause, and forward sequence.
- Add repository attributes that normalize text files to LF and explicitly
  classify prebuilt libraries and wheel packages as binary files.

### Changed

- Update the SDK version metadata and bundled Windows and Linux libraries to
  1.0.7.
- Refresh checked-in wheels for CPython 3.8 through 3.14 on Linux x86_64,
  Linux aarch64, and Windows AMD64.
- Document the Linux and Windows wheel build workflows in English and Chinese.
- Clarify main gait and sub-gait values, signed sub-gait interpretation, and
  Walk startup behavior.

### Fixed

- Fix an issue in the bundled SDK libraries where calling `setVelocity()`
  caused the selected gait setting to become ineffective.
- Fix swapped gait selection behavior in the bundled SDK libraries where
  `setPronk()` selected Bound and `setBound()` selected Pronk.

### Removed

- Remove the checked-in 1.0.6 wheel set. Version 1.0.7 wheels checked into this
  release cover Linux and Windows; macOS wheels remain available through the
  configured CI build workflow rather than the repository wheelhouse.

## [1.0.6] - 2026-07-08

### Added

- Add on-demand robot build version queries and cached version information
  after connection through the C++ API.
- Expose cached robot version information to Python and report it from the C++
  and Python examples.
- Add structured leg odometry containing body velocity, world position,
  orientation, and angular velocity in the C++ and Python APIs.
- Add CPython 3.8 through 3.14 wheel artifacts for Linux, Windows, and macOS.

### Changed

- Update the bundled SDK libraries and examples for the robot version query.

### Removed

- Remove `getCurrentVelocityBody()` and `getCurrentVelocityBodyArray()`, and
  replace the legacy velocity-only leg odometry API with structured leg
  odometry.

## 1.0.5 - 2026-07-07

### Added

- Add release wheel builds for Linux x86_64/aarch64, Windows AMD64, and macOS
  arm64, including CPython 3.14 support.
- Add velocity, leg odometry, and maximum velocity output to the examples.

### Changed

- Update bundled libraries and wheel artifacts for SDK 1.0.5.

## [1.0.4] - 2026-07-07

### Added

- Add native macOS arm64 SDK library and Python package support.
- Declare Python 3.8 and later support for the packaged bindings.

### Changed

- Extend CMake and Python packaging architecture detection to macOS arm64.
- Set the minimum macOS deployment target to macOS 11.0.

## 1.0.3 - 2026-06-18

### Added

- Add Python 3.8+ bindings, type information, and Python examples for state,
  motion-level, and joint-level control.
- Add Windows x86_64 DLL and import library packaging.
- Add configurable command-line connection options to the C++ and Python
  examples.
- Add separate English and Simplified Chinese documentation.

### Changed

- Make the CMake examples select the bundled SDK library for the current
  platform and architecture.

### Removed

- Remove the unsupported `setForwardFlip()` and `setBackFlip()` motion APIs.

## 1.0.2 - 2026-05-18

### Added

- Add explicit gait selection commands for Walk, Running, flips, Bipedal,
  inverted Bipedal, Pronk, Pace, and Bound.
- Add sub-gait feedback getters.

## 1.0.1 - 2026-05-07

### Added

- Add typed motion state, gait, and joint index definitions.
- Add typed state and gait getters alongside the raw-value APIs.
- Add `setZeroPositionsFlag()` for robot zero-position preparation.

### Changed

- Document IMU Euler-angle and quaternion component ordering.

## 1.0.0 - 2026-04-27

### Added

- Initial C++ SDK for robot state queries, motion-level control, and
  joint-level control.
- Initial Linux x86_64 and aarch64 shared libraries, CMake configuration, and
  C++ examples.

[Unreleased]: https://github.com/mirrormerobotics/bpx_sdk_open/compare/v1.0.10-rc.1...HEAD
[1.0.10-rc.1]: https://github.com/mirrormerobotics/bpx_sdk_open/compare/v1.0.9...v1.0.10-rc.1
[1.0.9]: https://github.com/mirrormerobotics/bpx_sdk_open/compare/v1.0.8...v1.0.9
[1.0.8]: https://github.com/mirrormerobotics/bpx_sdk_open/compare/v1.0.7...v1.0.8
[1.0.7]: https://github.com/mirrormerobotics/bpx_sdk_open/compare/v1.0.6...v1.0.7
[1.0.6]: https://github.com/mirrormerobotics/bpx_sdk_open/releases/tag/v1.0.6
[1.0.4]: https://github.com/mirrormerobotics/bpx_sdk_open/releases/tag/v1.0.4
