# BPX SDK Open

**English** | [中文](README.zh-CN.md)

`bpx_sdk_open` provides a lightweight C++ SDK for reading BPX robot state and sending motion-level or joint-level control commands.

SDK version: `1.0.11`

Documentation updated: `2026-10-28`

The SDK offers three usage modes:

| Mode                 | Description                                                                                                                      |
| -------------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| State Query Layer    | Subscribes to and reads robot state only; does not send control commands. Suitable for telemetry and state monitoring.           |
| Motion Control Layer | Sends high-level motion commands such as stand, sit, damping, and velocity control, while also reading robot state.              |
| Joint Control Layer  | Sends 12-DOF joint target and torque feedforward commands directly, while reading both standard state and high-rate joint state. |

The SDK interfaces and state fields are still in an early implementation stage. More APIs and readable robot state fields will be added over time.

## Before You Start

Update the SDK and ensure your robot firmware supports the features you intend to use.

## Directory Structure

```text
bpx_sdk_open/
  CMakeLists.txt
  include/
    bpx_sdk_config.h
    bpx_sdk_version.h
    motion_types.h
    request_robot_state.h
    motion_level_control.h
    joint_level_control.h
  lib/
    libbpx_sdk_x86_64.so
    libbpx_sdk_aarch64.so
    libbpx_sdk_aarch64.dylib
    bpx_sdk_x86_64.lib
  bin/
    bpx_sdk_x86_64.dll
  example/
    request_robot_state_example.cpp
    request_robot_state_example.py
    motion_level_control_example.cpp
    motion_level_control_example.py
    joint_level_control_example.cpp
    joint_level_control_example.py
  testcase/
    state_upload_rate_test.py
    tcp_reconnect_monitor.py
    tcp_reconnect_monitor_example.cpp
```

## General Configuration

Public configuration is defined in `include/bpx_sdk_config.h`.

| Name                                  | Description                                                |
| ------------------------------------- | ---------------------------------------------------------- |
| `DEFAULT_SERVER_IP`                   | Default robot IP used by the SDK.                          |
| `DEFAULT_CLIENT_ROBOT_STATE_UDP_PORT` | Local port for receiving robot state packets.              |
| `DEFAULT_CLIENT_JOINT_STATE_UDP_PORT` | Local port reserved for receiving joint state upload data. |

Command destination ports are fixed on the robot side and are not exposed as public SDK configuration.

The SDK default robot IP is `10.21.20.1` (RJ45 direct connection). To change the default IP, edit the
configuration in `include/bpx_sdk_config.h`, or call the `setRobotIp` API in your program to set the target
robot IP. See [Network Connection and IP](#network-connection-and-ip) for the IP to use with each connection method.

## Network Connection and IP

The SDK communicates with the BPX over Ethernet or WiFi. Choose the robot IP for your connection method and
configure it via `setRobotIp` or `DEFAULT_SERVER_IP` before connecting:

| Connection       | Description                                                                                 | Robot IP                                      |
| ---------------- | ------------------------------------------------------------------------------------------- | --------------------------------------------- |
| RJ45 direct      | Development host connected to the robot via an RJ45 cable                                   | `10.21.20.1`                                  |
| Robot AP hotspot | Development host connected to the robot's WiFi AP                                           | `10.21.40.1`                                  |
| Station WiFi     | Robot configured in Station mode and joined a WiFi network; host and robot on the same WiFi | Use the IP assigned to the robot on that WiFi |

## Motion State and Gait Types

Header: `include/motion_types.h`

The SDK provides type-safe motion state and gait enumerations for reading the current and previous motion state and gait.

Motion state `bpx_sdk::MotionState`:

| Enum Value   | Raw Value | Description              |
| ------------ | --------- | ------------------------ |
| `LyingDown`  | `0`       | Already lying down.      |
| `StandingUp` | `1`       | Standing up.             |
| `Passive`    | `2`       | Passive mode.            |
| `SitDown`    | `3`       | Lying down in progress.  |
| `Motion`     | `6`       | In motion.               |
| `ChargingSitDown` | `24` | Charging sit-down in progress. |

Gait `bpx_sdk::MotionGait`:

| Enum Value     | Raw Value |
| -------------- | --------- |
| `Walk`         | `0`       |
| `Bipedal`      | `3`       |
| `Flip`         | `4`       |
| `WalkPhase`    | `6`       |
| `PoseTracking` | `7`       |
| `Running`      | `8`       |
| `Tumbler` | `11` |
| `Jump` | `12` |

### Gaits and Sub-gaits

For some gaits, the main gait must be interpreted together with the sub-gait returned by `getSubGait()` to identify the specific action being executed:

| Main Gait      | Main Raw Value | Sub-gait      | Sub-gait Raw Value | Description       |
| -------------- | -------------- | ------------- | ------------------ | ----------------- |
| `Walk`         | `0`            | —             | —                  | Walking           |
| `Bipedal`      | `3`            | `F_HANDSTAND` | `-1`               | Inverted stance   |
| `Bipedal`      | `3`            | `B_HANDSTAND` | `1`                | Upright stance    |
| `Flip`         | `4`            | `GAIT_L_FLIP` | `-1`               | Left flip         |
| `Flip`         | `4`            | `GAIT_R_FLIP` | `-2`               | Right flip        |
| `WalkPhase`    | `6`            | `Pace`        | `2`                | Pace              |
| `WalkPhase`    | `6`            | `Bound`       | `1`                | Bound             |
| `WalkPhase`    | `6`            | `Pronk`       | `-1`               | Straight-leg jump |
| `PoseTracking` | `7`            | —             | —                  | In-place twisting |
| `Running`      | `8`            | `Run`         | `0`                | Running           |
| `Tumbler` | `11` | — | — | Self-righting recovery |
| `Jump` | `12` | `UpJump` | `0` | Upward jump |
| `Jump` | `12` | `FrontJump` | `1` | Forward jump |
| `Jump` | `12` | `BackJump` | `2` | Backward jump |
| `Jump` | `12` | `LeftJump` | `-1` | Leftward jump |
| `Jump` | `12` | `RightJump` | `-2` | Rightward jump |

An em dash indicates that the main gait does not require a sub-gait to distinguish the action. `getSubGait(uint8_t*)` and
`getSubGaitValue()` return `uint8_t`; negative raw sub-gait values must be interpreted as `int8_t`. For example,
`-1` and `-2` are transmitted as `255` and `254`, respectively:

```cpp
uint8_t raw_sub_gait = 0;
if (robot_state.getSubGait(&raw_sub_gait)) {
    const int8_t sub_gait = static_cast<int8_t>(raw_sub_gait);
}
```

Leg odometry `bpx_sdk::LegOdom`:

| Field              | Size | Description                                   |
| ------------------ | ---- | --------------------------------------------- |
| `velocity_body`    | `3`  | Body-frame linear velocity in m/s.            |
| `position`         | `3`  | World-frame position in m.                    |
| `orientation`      | `4`  | World-frame orientation quaternion, x/y/z/w.  |
| `angular_velocity` | `3`  | Body-frame angular velocity in rad/s.         |

### Gait Velocity Limits

Velocity commands use `[x, y, yaw]`, where `x` and `y` are body-frame linear velocities in m/s and `yaw` is yaw rate in rad/s. The following values are the controller-side command limits for each gait:

| Gait           | Velocity Limit `[x, y, yaw]` |
| -------------- | ---------------------------- |
| `Walk`         | `[1.5, 0.5, 2.0]`            |
| `Bipedal`      | `[0.8, 0.5, 1.5]`            |
| `Flip`         | `[0.0, 0.0, 0.0]`            |
| `WalkPhase`    | `[1.5, 1.0, 2.0]`            |
| `PoseTracking` | `[0.0, 0.0, 0.0]`            |
| `Running`      | `x: [-1.5, 3.0]`, `y: [-1.0, 1.0]`, `yaw: [-2.0, 2.0]` |
| `Tumbler`      | `[0.0, 0.0, 0.0]`            |

`Flip` does not use walking velocity commands. `Tumbler` requests self-righting with six zero axes and does not use velocity commands. `PoseTracking` limits pose tracking commands instead of body velocity commands. For `Walk`, `Bipedal`, `WalkPhase`, and `Running`, lateral velocity and yaw rate are further reduced as `abs(x)` increases, so the table lists the per-axis base limits.

## State Query Layer

Header: `include/request_robot_state.h`

Class: `bpx_sdk::RequestRobotState`

This layer subscribes to robot state data and provides read-only state query APIs. Use it when your application only needs telemetry and does not send control commands.

Joint and IMU data from the state query layer is updated at a lower rate, suitable for monitoring and low-frequency telemetry. For high-rate joint state, use the `HighRate` APIs in the joint control layer.

Connection and setup:

```cpp
bpx_sdk::RequestRobotState robot_state;
robot_state.setRobotIp(bpx_sdk::DEFAULT_SERVER_IP);
robot_state.setRobotStateUploadPort(bpx_sdk::DEFAULT_CLIENT_ROBOT_STATE_UDP_PORT);
robot_state.setTcpLocalPort(0);
robot_state.setRobotStateUploadRate(100);

if (!robot_state.connect()) {
    return 1;
}
```

`setRobotStateUploadRate()` requests an upper limit for ordinary UDP state packets sent from the robot to the SDK client; it does not control the SDK-to-robot motion command rate. Call it before `connect()`. The valid range is 1--200 Hz, the default is 100 Hz, and the robot falls back to 100 Hz for invalid values. A change made while connected takes effect after disconnecting and reconnecting.

`connect()` starts a desired connection. If the robot is temporarily unavailable, the SDK reconnects in the background with bounded backoff from 100 ms to 2 s; use `isConnected()` for the live TCP state. The connection is reported live only after the first complete robot response. A bounded response deadline also discards a half-open socket and starts reconnection when Wi-Fi loss, power loss, or a hard robot reboot produces no TCP FIN/RST. Explicit `disconnect()`, object destruction, or process exit stops all retries. Every confirmed connection and background reconnection automatically sends one time-sync request on the active TCP session. Reconnection never restores a pre-disconnect non-zero motion or joint target.

Each new motion-control TCP session first sends a complete zero command. The application must explicitly call a new motion-control API after that baseline before a non-zero target can resume. Joint control never replays the old frame after reconnect: call `setJointCommand()` with a new complete frame (or `setZeroJointCommand()`) first; partial joint setters return `false` until then.

Each ordinary state group keeps its native cap, so its effective rate is `min(requested rate, native cap)`: joints 1000 Hz, IMU 200 Hz, leg odometry 50 Hz, motion state 10 Hz, and battery/temperature 1 Hz. The joint-control `HighRate` channel is independent of this setting. Command-line examples accept the same setting through `--state-rate 50`.

### Automatic robot time synchronization after connection

Applications only need to call `connect()`. After the first complete subscription response confirms a live connection, the SDK automatically sends the development host's current epoch on the same TCP session. Every successful background reconnection sends it again. The auxiliary request does not change the active control mode, and a time-sync policy or application failure does not turn an established control connection into a connection failure.

The robot uses one absolute-offset threshold of ten minutes. It does not step the clock at or below ten minutes; above ten minutes it applies SDK time to both Linux `CLOCK_REALTIME` and the UTC hardware RTC. NTP state is reported for diagnostics but no longer blocks an SDK correction above the threshold. Threshold skips, permission errors, and partial RTC failures are returned and logged by the robot. The SDK no longer exposes an explicit time-sync API; normal applications only need to call `connect()`.

The packaged robot configuration enables automatic time sync for a trusted robot network. The current protocol has no authentication, so untrusted networks must disable `sdk_time_sync_enabled` in robot configuration. Error code `9` means an unknown NTP state and `10` means an RTC write/readback failure. Protocol revision 2 intentionally rejects revision-1 clients because the old response cannot safely express policy skips or partial RTC application.

Automatic time synchronization requires a supporting SDK version for your platform.

Robot control timeouts and arbitration continue to use a monotonic clock. The API does not synchronize MCU, IMU, or joint-device clocks and does not replace continuous NTP/chrony service.

Main state APIs:

| API                                   | Description                       |
| ------------------------------------- | --------------------------------- |
| `getJointPosition(float[12])`         | Joint positions in radians.       |
| `getJointVelocity(float[12])`         | Joint velocities in rad/s.        |
| `getJointTorque(float[12])`           | Joint torques.                    |
| `getImuRpy(float[3])`                 | Body roll, pitch, and yaw.        |
| `getImuQuat(float[4])`                | Body orientation quaternion.      |
| `getImuAcc(float[3])`                 | IMU linear acceleration.          |
| `getImuOmega(float[3])`               | IMU angular velocity.             |
| `getLegOdom(LegOdom*)`                | Leg odometry data.                |
| `getCurrentMotionState(uint8_t*)`     | Current motion state.             |
| `getCurrentGait(uint8_t*)`            | Current gait.                     |
| `getLastMotionState(uint8_t*)`        | Previous motion state.            |
| `getLastGait(uint8_t*)`               | Previous gait.                    |
| `getSubGait(uint8_t*)`                | Current sub-gait.                 |
| `getCurrentMotionState(MotionState*)` | Current motion state as an enum.  |
| `getCurrentGait(MotionGait*)`         | Current gait as an enum.          |
| `getLastMotionState(MotionState*)`    | Previous motion state as an enum. |
| `getLastGait(MotionGait*)`            | Previous gait as an enum.         |
| `getMaxVelocity(float[3])`            | Current maximum velocity limits.  |
| `getBatteryLevel(uint8_t*)`           | Battery level as a percentage.    |
| `getBatteryCurrent(float*)`           | Battery current.                  |
| `getMotorTemperature(float[12])`      | Motor temperatures.               |
| `getDriverTemperature(float[12])`     | Driver temperatures.              |
| `getJointStateTimestamp(uint32_t*)`   | Latest joint state timestamp.     |
| `getImuTimestamp(uint32_t*)`          | Latest IMU data timestamp.        |
| `getOdometryTimestamp(uint32_t*)`     | Latest odometry timestamp.        |
| `getMotionStateTimestamp(uint32_t*)`  | Latest motion state timestamp.    |
| `getBatteryTimestamp(uint32_t*)`      | Latest battery state timestamp.   |

The SDK also provides optional helper APIs that return arrays, structs, or scalar values, such as `getJointPositionArray()`, `getImuRpyArray()`,
`getLegOdomValue()`,
`getSubGaitValue()`, and `getBatteryLevelValue()`. These return `std::optional` and let callers check read success more concisely. Motion state and gait also provide
`getCurrentMotionStateEnum()`, `getCurrentGaitEnum()`, `getLastMotionStateEnum()`, and `getLastGaitEnum()`,
which return `MotionState` or `MotionGait` directly.

Example: `example/request_robot_state_example.cpp`

### Robot Serial Number and Model (C++)

`RequestRobotState` and its derived classes `MotionLevelControl` and `JointLevelControl` provide:

| API | Description |
| --- | --- |
| `getRobotSerialNumber(std::string*)` | Reads the original whole-robot SN; returns `true` on success. |
| `getRobotModel(RobotModel*)` | Reads the model identified by the robot hardware. |
| `getRobotSerialNumberValue()` | Returns `std::optional<std::string>`. |
| `getRobotModelValue()` | Returns `std::optional<RobotModel>`. |
| `getControlMode(ControlMode*)` | Reads the current control mode; returns `true` on success. |
| `getControlModeValue()` | Returns `std::optional<ControlMode>`. |

`RobotModel` is defined in `include/motion_types.h`:

| Enum | SDK Enum Value |
| --- | --- |
| `Unknown` | `0` |
| `BPX` | `1` |
| `BPXPro` | `2` |
| `BPW` | `3` |

The SN identifies the whole robot.

```cpp
// Call from the state-reading loop after connect().
auto sn = robot_state.getRobotSerialNumberValue();
auto model = robot_state.getRobotModelValue();
if (sn && model) {
    // An empty *sn means the robot could not read its SN; *model is the detected model.
}
```

`connect()` attempts to retrieve identity synchronously before returning. Getters return `false` / `std::nullopt` if the query fails or the firmware does not support it.
An empty SN means the serial number is unavailable; `Unknown` means the model is unknown.
Identity is printed once when successfully retrieved during connection setup or automatic reconnection:

```text
robot identity: SN=BPX-example-SN, model=BPX-Pro
```

The current control mode updates with state feedback and is available through `getControlMode()`
or `getControlModeValue()`. `ControlMode` values are `Unknown=0`, `RemoteControl=1` (app/remote
controller), and `Navigator=2` (navigation). Missing feedback or disconnection returns
`false` / `std::nullopt`.

## Motion Control Layer

Header: `include/motion_level_control.h`

Class: `bpx_sdk::MotionLevelControl`

This layer sends high-level robot control commands. It inherits from `RequestRobotState`, so a single object can both send motion commands and read robot state.

Connection and setup:

```cpp
bpx_sdk::MotionLevelControl motion;
motion.setRobotIp(bpx_sdk::DEFAULT_SERVER_IP);
motion.setRobotStateUploadPort(bpx_sdk::DEFAULT_CLIENT_ROBOT_STATE_UDP_PORT);
motion.setTcpLocalPort(0);
motion.setRobotStateUploadRate(100);
motion.setMotionCommandRate(50);

if (!motion.connect()) {
    return 1;
}
```

Control APIs:

| API                                        | Description                                                                            |
| ------------------------------------------ | -------------------------------------------------------------------------------------- |
| `setMotionCommandRate(uint16_t rate_hz)`   | Sets the periodic command send rate.                                                   |
| `setVelocityControlFlag(bool enabled)`     | Enables or disables the velocity control flag in outgoing packets.                     |
| `setZeroPositionsFlag()`                   | Sets the zero-position flag. Example programs send this before formal motion commands. |
| `setWalk()`                                | Switches to normal walking gait.                                                       |
| `setRunning()`                             | Switches to running gait.                                                              |
| `setTumbler()` | Requests BPX/BPX-Pro self-righting recovery while lying down; returns whether the request was sent. |
| `setLeftFlip()`                            | Requests a left flip.                                                                  |
| `setRightFlip()`                           | Requests a right flip.                                                                 |
| `setBipedal()`                             | Switches to bipedal gait.                                                              |
| `setInvBipedal()`                          | Switches to inverted bipedal gait.                                                     |
| `setUpJump()` | Requests a BPX upward jump. |
| `setFrontJump()` | Requests a BPX forward jump. |
| `setBackJump()` | Requests a BPX backward jump. |
| `setLeftJump()` | Requests a BPX leftward jump. |
| `setRightJump()` | Requests a BPX rightward jump. |
| `setPronk()`                               | Switches to Pronk jumping gait.                                                        |
| `setPace()`                                | Switches to Pace gait.                                                                 |
| `setBound()`                               | Switches to Bound gait.                                                                |
| `setVelocity(float x, float y, float yaw)` | Sends body velocity and yaw rate commands.                                             |
| `setStandUp()`                             | Requests stand mode.                                                                   |
| `setSitDown()`                             | Requests sit mode.                                                                     |
| `setChargingSitDown()` | Requests charging sit-down; returns whether the command was sent. |
| `setDamping()`                             | Requests joint damping mode.                                                           |

The unimplemented Python placeholder `setUpright()` has been removed. Use
`setStandUp()` for four-legged standing, or `setBipedal()` / `setInvBipedal()`
for BPX bipedal gaits.

Call `setChargingSitDown()` after connection is ready and the robot is in motion mode.
It sends target state `24`, Walk gait, and zero velocity; the robot still applies its
stationary-time and state-transition checks. `true` means sent, not completed.
While executing, `getCurrentMotionState()` reports `MotionState::ChargingSitDown`.
After entering damping, the current state is `Passive` and `getLastMotionState()`
reports `ChargingSitDown`. These are existing motion-state feedback fields (up to
10 Hz). Previous state is not a completion flag: a damping request or safety protection
can also interrupt the action. Charger insertion is reported separately by `getChargerIn1/2()`, and the hard estop switch state by `getHardEstop()`.

Call `setTumbler()` on BPX/BPX-Pro while the robot is in `MotionState::LyingDown`
to request self-righting recovery. Each call sends one recovery request with six
zero axes and sub-gait 0; the SDK's periodic resends do not retrigger the
request. `true` means the request was sent, not that recovery completed, and
robot-side safety checks still apply. During execution the gait feedback reports
`MotionGait::Tumbler` (`11`).

The five directional jump APIs are available for BPX. Call them after connection, while the robot
is in motion mode and commanded velocity is zero. Each call requests one jump.
`setPronk()` selects the Pronk gait. `setBipedal()` / `setInvBipedal()` are available for BPX;
BPX-Pro does not support upright or inverted stance.


Before calling `setZeroPositionsFlag()`, ensure the robot's feet, shanks, and the joints between shanks and thighs are all in contact with the ground.
`example/motion_level_control_example.cpp` sends a zeroing command at startup by default, so before running
`motion_level_control_example`, make sure the BPX is already in the required zeroing pose.

Example: `example/motion_level_control_example.cpp`

### Pose Tracking

BPX and BPX-Pro support `setPoseTracking()` while already in `MotionState::Motion`. The request clears all six axes so the controller can change gait. A `true` result means sent; wait for `getCurrentGait()` / `getCurrentGaitEnum()` to report `PoseTracking` before sending nonzero input.

`setPose(x, y, z, r)` accepts four finite normalized values in `[-1, 1]` in both C++ and Python; `r` is the yaw input. These are not pose values in meters or radians. The SDK fills the wire channels as `[x, y, r, z, 0, 0]`, keeping the two unexposed channels at zero.

Before per-axis scaling, the current controller maps these inputs to `[-y, x, r, 0, 0, z]`, preserving the existing X/Y swap and resulting X inversion. The SDK does not duplicate this mapping.

- `setPose()` always uses normalized control without changing the velocity-control preference set for ordinary motion. Calling `setVelocityControlFlag(false)` first is unnecessary.
- It returns `false` for invalid values, an unavailable connection, or unless pose tracking was selected in the current connection and no other gait has since been selected. Rejected input preserves the last valid command. Python raises `TypeError` for incorrect argument counts or types.
- The SDK periodically resends the latest input until replaced. `setPose()` does not wait for gait feedback. Use it for pose input and use `setVelocity()` for walking velocity.
- `setPose(0, 0, 0, 0)` requests neutral input. Leaving pose tracking with `setWalk()` or another gait selector clears all six axes and restores the previous velocity-control preference. Wait for gait feedback before sending nonzero walking velocity.
- Disconnect clears all six axes. Select pose tracking again after reconnecting.

Complete examples: [C++](example/pose_tracking_example.cpp), [Python](example/pose_tracking_example.py). The bundled Linux x86_64/aarch64, Windows x86_64, and macOS arm64 libraries include these APIs.

Both examples use the shared command-line parser and defaults, supporting `--robot-ip`, `--state-port` (alias `--robot-state-port`), `--tcp-local-port`, `--state-rate`, and `-h/--help`:

After connecting, the examples call `setStandUp()` and wait for `Motion` feedback before entering pose tracking and sweeping `x → y → z → r` one channel at a time. Each channel first holds `-1000` for one second, then increases linearly to `1000` over three seconds, with updates every 20 ms (151 values). All other channels stay zero. Values are divided by `1000` before sending normalized SDK input; the terminal prints the original `x/y/z/r` values, where `r` is the SDK `yaw` channel. The `1000` endpoint is held for one second, followed by zero input for one second before the next channel. The final channel also pauses at zero for one second before returning to `Walk`. The examples then call `setSitDown()` to lie down and wait for `LyingDown` feedback before disconnecting and exiting. Standing and lying down each have a ten-second completion timeout; a timeout or send failure stops the sequence and disconnects.

```bash
./build/pose_tracking_example --robot-ip <ROBOT_IP> --state-rate 100
python3 example/pose_tracking_example.py --robot-ip <ROBOT_IP> --state-rate 100
```

```cpp
// Request after connecting, with the robot already in Motion state.
const bool sent = motion.setPoseTracking();
// In a later state loop, wait for pose-tracking feedback before nonzero input.
if (sent && motion.getCurrentGaitEnum() == bpx_sdk::MotionGait::PoseTracking) {
    motion.setPose(0.1f, -0.1f, 0.0f, 0.0f);
}
```

## Joint Control Layer

Header: `include/joint_level_control.h`

Class: `bpx_sdk::JointLevelControl`

This layer sends joint control commands directly. It also inherits all state query APIs from `RequestRobotState`, so applications can send joint targets and read robot state in the same loop.

A wired connection is recommended for joint-level development.

When the robot is in `DevelopingState`, high-rate joint state is uploaded through the joint state channel. `JointLevelControl` receives this stream in the background and exposes the latest packets through `HighRate` APIs.

The joint control model is:

```text
torque = kp * (target_position - current_position)
       + kd * (target_velocity - current_velocity)
       + torque_feed_forward
```

Connection and setup:

```cpp
bpx_sdk::JointLevelControl joint;
joint.setRobotIp(bpx_sdk::DEFAULT_SERVER_IP);
joint.setRobotStateUploadPort(bpx_sdk::DEFAULT_CLIENT_ROBOT_STATE_UDP_PORT);
joint.setTcpLocalPort(0);
joint.setRobotStateUploadRate(100);

if (!joint.connect()) {
    return 1;
}
```

Control APIs:

| API                                      | Description                                |
| ---------------------------------------- | ------------------------------------------ |
| `setJointCommand(kp, pos, kd, vel, tff)` | Sends a full 12-DOF joint command.         |
| `setJointKp(kp)`                         | Updates and sends joint `kp`.              |
| `setJointPosition(pos)`                  | Updates and sends target joint positions.  |
| `setJointKd(kd)`                         | Updates and sends joint `kd`.              |
| `setJointVelocity(vel)`                  | Updates and sends target joint velocities. |
| `setJointTorqueFeedForward(tff)`         | Updates and sends feedforward torques.     |
| `setZeroJointCommand()`                  | Sends a zeroed joint command.              |

High-rate state APIs:

| API                                      | Description                                               |
| ---------------------------------------- | --------------------------------------------------------- |
| `getJointPositionHighRate(float[12])`    | Latest high-rate joint positions from `DevelopingState`.  |
| `getJointVelocityHighRate(float[12])`    | Latest high-rate joint velocities from `DevelopingState`. |
| `getJointTorqueHighRate(float[12])`      | Latest high-rate joint torques from `DevelopingState`.    |
| `getImuRpyHighRate(float[3])`            | Latest high-rate IMU roll, pitch, and yaw.                |
| `getImuQuatHighRate(float[4])`           | Latest high-rate IMU quaternion.                          |
| `getImuAccHighRate(float[3])`            | Latest high-rate IMU acceleration.                        |
| `getImuOmegaHighRate(float[3])`          | Latest high-rate IMU angular velocity.                    |
| `getJointStateTimestampHighRate(float*)` | Timestamp carried by the latest high-rate packet.         |
| `getJointStateSeqHighRate(uint32_t*)`    | Sequence number of the latest high-rate packet.           |

`example/joint_level_control_example.cpp` demonstrates how to send joint commands and periodically print motion state, gait, battery level, IMU RPY, and high-rate position, velocity, and torque for the first six joints.

## Requirements and Installation

Requirements:

- Ubuntu 22.04 or later on Linux.
- macOS aarch64 when using the macOS SDK binary.
- Windows x86_64 when using the Windows SDK binaries.
- Python 3.8 or later when using the Python bindings. Windows requires 64-bit Python.

Installation:

Build the SDK examples inside the `bpx_sdk_open` directory:

```bash
mkdir build
cd build
cmake ..
make
```

On Windows, open the x64 Native Tools Command Prompt for VS, then build the examples from the `bpx_sdk_open` directory:

```bat
cmake -S . -B build\windows_x64_examples -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build\windows_x64_examples
```

After building, the example executables are located in `build/`.

CMake automatically links the shared library for the current system architecture. Modify `CMakeLists.txt` if you have other linking or build requirements.

### Python Jump and State APIs

`RequestRobotState`, `MotionLevelControl`, and `JointLevelControl` provide
`getRobotSerialNumber()` and `getRobotModel()`. They return a string and a model integer,
respectively, or `None` when identity is unavailable. An empty string means the SN is unavailable.

```python
# Use in the state-reading loop after connecting robot.
sn = robot.getRobotSerialNumber()
model = robot.getRobotModel()
if sn is not None and model is not None:
    print(sn, bpx_sdk.RobotModel(model).name)
```

Model values are `RobotModel.Unknown = 0`, `BPX = 1`, `BPXPro = 2`, and `BPW = 3`.
Use `model is not None` to check availability, since `Unknown` has value `0`.

All three classes also provide `getControlMode()`, returning a control-mode integer or `None`
when feedback is unavailable or the connection is lost.

```python
mode = robot.getControlMode()
if mode is not None:
    print(bpx_sdk.ControlMode(mode).name)
```

`ControlMode` values are `Unknown=0`, `RemoteControl=1`, and `Navigator=2`.

`getChargerIn1()` and `getChargerIn2()` return `0` (removed), `1` (inserted), or `None`
when feedback is unavailable. The C++ and Python examples display both values in their state output.

`getHardEstop()` returns `1` while the hard estop switch on the robot is pressed (all
joints unpowered), `0` when released, and `None` before 1 Hz feedback arrives. It is
carried on bit 2 of the 1 Hz status flags and requires robot firmware that publishes
it. The examples include it in the same state output line.

`getFaultState()` returns the full active-fault snapshot from a dedicated 1 Hz UDP
channel (head `0x00F0`): `{'entry_count': int, 'truncated': bool, 'entries':
[{'code': int, 'occurred_t_ms': int}]}` in Python, a `RobotFaultState` struct in
C++. Up to 32 entries are sorted severity-descending, so `entries[0]` is the
top-priority fault; `entry_count=0` snapshots keep arriving as a heartbeat while
the robot is fault-free. `None`/false only means no fault packet has been received
yet (e.g. an older robot). `occurred_t_ms` is Unix epoch milliseconds; use the
bitfield helpers in `fault_code.h` (`DecodeFaultSeverity/Module/SubModule/SubCode`)
to render a code. The examples show a `faults=<count>[+truncated] top=0x…`
summary. Requires robot firmware that publishes the channel; `getFaultTimestamp()`
returns the snapshot's receive time on the monotonic clock.

#### Currently reported fault codes

The table below lists every fault code the current robot firmware actually
detects and reports on the fault channel (verified against the detection
code paths). Codes defined in the enums but not listed here are reserved for
future detections and never appear on the wire today. `sub_module` is the
joint index (0-11) unless noted; severity is embedded in the code
(only `1=Warning` and `2=Error` are emitted today).

| Module | Sub-code | Meaning | Sub-module | Typical severity |
| --- | --- | --- | --- | --- |
| `0x02` Joint | `0x0001` | Joint position out of range | joint index | Error |
| `0x02` Joint | `0x0002` | Joint velocity out of range | joint index | Error |
| `0x02` Joint | `0x0004` | No joint feedback data | joint index | Error |
| `0x02` Joint | `0x0005` | Motor over temperature | hottest joint | Warning (early band) / Error (protection band) |
| `0x02` Joint | `0x0007` | Invalid joint position feedback (NaN) | joint index | Error |
| `0x02` Joint | `0x0008` | Invalid joint velocity feedback (NaN) | joint index | Error |
| `0x02` Joint | `0x000A` | Joint communication lost | joint index | Error |
| `0x02` Joint | `0x000B` | Joint did not reach the commanded pose | joint index | Error (raised while standing up) |
| `0x02` Joint | `0x000C` | Knee motor model mismatch | `0xFF` | Error |
| `0x02` Joint | `0x000D` | Joint assembled in the wrong position | joint index | Error |
| `0x02` Joint | `0x000E` | Motor temperature abnormal relative to same-type joints | joint index | Warning |
| `0x02` Joint | `0x000F` | Duplicated CAN ID | joint index | Error |
| `0x02` Joint | `0x0010` | Invalid joint serial number | joint index | Error |
| `0x02` Joint | `0x0011` | All joint communications lost at once | `0xFF` | Error |
| `0x03` Driver | `0x0001` | Driver overvoltage | joint index | Warning / Error (soft-estop class) |
| `0x03` Driver | `0x0002` | Driver undervoltage | joint index | Warning / Error (soft-estop class) |
| `0x03` Driver | `0x0003` | Driver over temperature | joint index | Warning (early band) / Error (protection band) |
| `0x03` Driver | `0x0004` | Motor stall protection | joint index | Warning / Error (soft-estop class) |
| `0x03` Driver | `0x0005` | Driver temperature abnormal relative to same-type joints | joint index | Error |
| `0x03` Driver | `0x0006` | Encoder not calibrated | joint index | Warning / Error (soft-estop class) |
| `0x03` Driver | `0x0007` | ADC self-test failed | joint index | Warning / Error (soft-estop class) |
| `0x04` IMU | `0x0001` | Acceleration out of range | `0xFF` | Warning |
| `0x04` IMU | `0x0004` | No acceleration data | `0xFF` | Error |
| `0x05` Battery | `0x0004` | Battery level too low | `0xFF` | Error |

Not reported today (enum-reserved only): all `0x01` Motion codes; Joint
`0x0003/0x0006/0x0009`; IMU `0x0002/0x0003`; Battery
`0x0001/0x0002/0x0003/0x0005/0x0006`.

`MotionLevelControl` provides five jump methods, all returning `None`:

| Method | Action |
| --- | --- |
| `setUpJump()` | Upward jump |
| `setFrontJump()` | Forward jump |
| `setBackJump()` | Backward jump |
| `setLeftJump()` | Leftward jump |
| `setRightJump()` | Rightward jump |

These actions are available for BPX. Call them after connection, while the robot is in motion mode
and commanded velocity is zero. Each call requests one jump. The gait is `MotionGait.Jump = 12`.
Install the updated Python package and ensure the robot firmware supports the features you use.

### Python Bindings

The Python bindings are provided by the `bpx_sdk` package and link directly against the C++ shared library shipped in this repository. Install them with:

```bash
python3 -m pip install .
```

The Windows Python bindings support x86_64 only. During the build, `lib/bpx_sdk_x86_64.lib` is used to link the native extension, and `bin/bpx_sdk_x86_64.dll` is installed into the Python package.

On Linux, the Python build selects `lib/libbpx_sdk_<arch>.so` for the current machine architecture. On macOS, the Python build selects `lib/libbpx_sdk_<arch>.dylib`; the repository currently ships `lib/libbpx_sdk_aarch64.dylib` for Apple Silicon. You can override architecture detection with `BPX_SDK_ARCH=x86_64` or `BPX_SDK_ARCH=aarch64` when building.

Installing from source with `python3 -m pip install .` compiles the native Python extension locally, so the build machine must have a C++17 compiler and Python development headers. Windows source builds require Microsoft C++ Build Tools. Users who install a prebuilt wheel do not need the compiler toolchain.

#### Building Wheels

Build a wheel for the current OS and Python interpreter:

```bash
python3 scripts/build_wheels.py --out-dir wheelhouse
```

Build release wheels with `cibuildwheel` for all configured CPython versions on the current OS:

```bash
python3.11 scripts/build_wheels.py --cibuildwheel --out-dir wheelhouse
```

Running `cibuildwheel` requires Python 3.11 or newer as the host interpreter.

On Linux, the environment setup and release build can be run with one command:

```bash
./scripts/build_linux_wheels.sh
```

This command uses `uv` to create or reuse `.venv-cibw-linux` with Python 3.11 and
`pip`, installs or upgrades `cibuildwheel<4`, and writes the wheels to
`wheelhouse`. Pass a different output directory as the first argument if needed.

On Windows, use the equivalent PowerShell script:

```powershell
.\scripts\build_windows_wheels.ps1
```

It uses `uv` to create or reuse `.venv-cibw-win` with Python 3.11 and writes the
wheels to `wheelhouse`. Pass a different output directory as the first positional
argument:

```powershell
.\scripts\build_windows_wheels.ps1 dist\wheels
```

To ignore proxy environment variables and the Windows system proxy for this build,
use `-DisableProxy`. The original proxy settings are restored when the script exits:

```powershell
.\scripts\build_windows_wheels.ps1 -DisableProxy
```

On local macOS machines, `cibuildwheel` only uses python.org CPython framework installs.
The build script skips configured CPython versions that are not installed locally. In
GitHub Actions, the full configured CPython matrix is built.

The GitHub Actions workflow in `.github/workflows/build-wheels.yml` builds wheels in four parallel jobs: Windows AMD64, Linux x86_64, Linux aarch64, and macOS arm64. Linux uses native `ubuntu-22.04` and `ubuntu-22.04-arm` runners, one architecture per job, without QEMU emulation. Run it manually from the Actions tab or push a `v*` tag. The generated wheels are uploaded as workflow artifacts and can be installed with:

```bash
pip3 install --no-index --find-links wheelhouse bpx-sdk-open
```

If pip is too old to recognize newer wheel platform tags, the install may fail with:

```text
Looking in links: wheelhouse
ERROR: Could not find a version that satisfies the requirement bpx-sdk-open (from versions: none)
ERROR: No matching distribution found for bpx-sdk-open
```

Upgrade pip and retry:

```bash
pip3 install --upgrade pip
```

After installation, verify the package import with:

```bash
python3 -c "import bpx_sdk; print('ok')"
```

After installation, use the same three client types exposed by the C++ SDK:

```python
import bpx_sdk

robot_state = bpx_sdk.RequestRobotState()
robot_state.setRobotIp(bpx_sdk.DEFAULT_SERVER_IP)
robot_state.setRobotStateUploadPort(bpx_sdk.DEFAULT_CLIENT_ROBOT_STATE_UDP_PORT)
robot_state.setTcpLocalPort(0)
robot_state.setRobotStateUploadRate(100)

if robot_state.connect():
    print(robot_state.getImuRpy())
    print(robot_state.getBatteryLevel())
    robot_state.disconnect()
```

Python adds `getFaultState() -> Optional[FaultState]` and
`getFaultTimestamp() -> Optional[int]` on all state classes, plus the
`FaultEntry` / `FaultState` TypedDicts. See the bundled `include/fault_code.h`
for the 64-bit fault-code bitfields. Requires robot firmware that publishes the
fault channel.

Python 1.0.10 adds `MotionLevelControl.setChargingSitDown() -> bool` and
`MotionState.ChargingSitDown = 24`. Updating the native SDK headers/libraries does
not rebuild an installed Python extension: rebuild/reinstall the package with
`python3 -m pip install --upgrade --force-reinstall .`, or install a 1.0.10 wheel.
State getters continue to return integers or `None` and can be compared with the enum.

```python
# motion is an already-connected MotionLevelControl; robot is in motion mode.
sent = motion.setChargingSitDown()
print("command sent:", sent)
# Poll feedback during subsequent control-loop iterations.
current = motion.getCurrentMotionState()
previous = motion.getLastMotionState()
if current == bpx_sdk.MotionState.ChargingSitDown:
    print("charging sit-down in progress")
print("previous state:", previous)
```

Python 1.0.11 adds `MotionLevelControl.setTumbler() -> bool` and
`MotionGait.Tumbler = 11` for the BPX/BPX-Pro self-righting request while the
robot is in `MotionState.LyingDown`:

```python
# motion is an already-connected MotionLevelControl; robot is lying down.
sent = motion.setTumbler()
print("recovery request sent:", sent)
# Poll feedback during subsequent control-loop iterations.
if motion.getCurrentGait() == bpx_sdk.MotionGait.Tumbler:
    print("self-righting in progress")
```

Python method names match the C++ SDK. Read APIs return a
`list`, `dict`, `int`, or `float` on success, and `None` when no valid data has been received yet. `getLegOdom()` returns a dict with `velocity_body`, `position`, `orientation`, and `angular_velocity` lists. Joint control APIs accept Python sequences with exactly 12 numbers:

```python
joint = bpx_sdk.JointLevelControl()
zeros = [0.0] * bpx_sdk.JOINT_COUNT
kp = [100.0] * bpx_sdk.JOINT_COUNT
kd = [2.0] * bpx_sdk.JOINT_COUNT
joint.setJointCommand(kp, zeros, kd, zeros, zeros)
```

Examples:
`example/request_robot_state_example.py`,
`example/motion_level_control_example.py`,
`example/joint_level_control_example.py`



## Running

The SDK communicates with the BPX over the network. Choose the correct robot IP per [Network Connection and IP](#network-connection-and-ip); if it differs from `DEFAULT_SERVER_IP`, call `setRobotIp` before connecting. Before running your program, `ping` that IP from the development host to verify network connectivity, then call `connect()`.

### Server/client examples

`motion_level_control_server_example` owns the SDK connection and accepts text commands from a client. C++ and Python clients and servers are interchangeable. Start one server, then run a client in another terminal:

```bash
# C++ (from the repository root)
./build/motion_level_control_server_example --robot-ip 192.168.1.143
./build/motion_level_control_client_example status

# Python (install the Python SDK first; use instead of the C++ server above)
python3 example/motion_level_control_server_example.py --robot-ip 192.168.1.143
python3 example/motion_level_control_client_example.py status
```

The server listens on `127.0.0.1:50051` by default. For separate machines, set the server's `--listen-ip` and the client's `--server-ip`; use `--listen-port` and `--server-port` to change the port. This example has no authentication and is intended for trusted networks. The server handles client connections sequentially; running a client without a command starts interactive mode.

`status` reports connection status, SN, model, ControlMode, current/previous motion state, gait, maximum velocity, battery level/current, and both charger inputs. Missing fields show `<unavailable>`. The server prints status every second with a local timestamp including milliseconds, keeping `battery_level`, `battery_current`, `charger_in1`, `charger_in2`, and `hard_estop` on the same line.

Commands include `zero`, `stand`, `tumbler`, `sit`, `charging-sit`, `damping`, `stop`, `velocity X Y YAW`, `velocity-control on|off`, and `gait NAME`. For BPX, `jump up|front|back|left|right` selects the five jump directions. Available actions depend on the robot model. Use the client’s `-h`/`--help` to view all commands without connecting. While connected, use `help` to list commands and gait/jump choices; `quit` (or `shutdown`) stops the server. An `OK` response means the server handled the command, not that the robot completed the action.

`charging-sit` calls `setChargingSitDown()` in both server implementations.
Query `status` to read `motion_state=24(ChargingSitDown)` while executing,
or `last_motion_state=24(ChargingSitDown)` after exiting to damping.

```bash
./build/motion_level_control_client_example charging-sit
./build/motion_level_control_client_example status
python3 example/motion_level_control_client_example.py charging-sit
python3 example/motion_level_control_client_example.py status
```

`tumbler` calls `setTumbler()` in both server implementations. Send it while the
robot is lying down; `status` reports `gait=11(Tumbler)` during recovery.

## License

BPX SDK Open is licensed under the Apache License 2.0. The license covers the
source code, headers, examples, Python bindings, precompiled BPX SDK libraries,
and wheels distributed in this repository. See [LICENSE](LICENSE),
[NOTICE](NOTICE), and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
