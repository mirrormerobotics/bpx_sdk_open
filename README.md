# BPX SDK Open

**English** | [中文](README.zh-CN.md)

`bpx_sdk_open` provides a lightweight C++ SDK for reading BPX robot state and sending motion-level or joint-level control commands.

Current version: `1.0.6`

The SDK offers three usage modes:

| Mode                 | Description                                                                                                                      |
| -------------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| State Query Layer    | Subscribes to and reads robot state only; does not send control commands. Suitable for telemetry and state monitoring.           |
| Motion Control Layer | Sends high-level motion commands such as stand, sit, damping, and velocity control, while also reading robot state.              |
| Joint Control Layer  | Sends 12-DOF joint target and torque feedforward commands directly, while reading both standard state and high-rate joint state. |

The SDK interfaces and state fields are still in an early implementation stage. More APIs and readable robot state fields will be added over time.

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

| Enum Value   | Raw Value | Description   |
| ------------ | --------- | ------------- |
| `LyingDown`  | `0`       | Lying down.   |
| `StandingUp` | `1`       | Standing up.  |
| `Passive`    | `2`       | Passive mode. |
| `SitDown`    | `3`       | Sitting down. |
| `Motion`     | `6`       | In motion.    |

Gait `bpx_sdk::MotionGait`:

| Enum Value     | Raw Value |
| -------------- | --------- |
| `Walk`         | `0`       |
| `Bipedal`      | `3`       |
| `Flip`         | `4`       |
| `WalkPhase`    | `6`       |
| `PoseTracking` | `7`       |
| `Running`      | `8`       |

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

`Flip` does not use walking velocity commands. `PoseTracking` limits pose tracking commands instead of body velocity commands. For `Walk`, `Bipedal`, `WalkPhase`, and `Running`, lateral velocity and yaw rate are further reduced as `abs(x)` increases, so the table lists the per-axis base limits.

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
| `getCurrentVelocityBody(float[3])`    | Current body-frame velocity.      |
| `getLegOdom(float[3])`                | Leg odometry data.                |
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

The SDK also provides optional helper APIs that return arrays or scalar values, such as `getJointPositionArray()`, `getImuRpyArray()`,
`getSubGaitValue()`, and `getBatteryLevelValue()`. These return `std::optional` and let callers check read success more concisely. Motion state and gait also provide
`getCurrentMotionStateEnum()`, `getCurrentGaitEnum()`, `getLastMotionStateEnum()`, and `getLastGaitEnum()`,
which return `MotionState` or `MotionGait` directly.

Example: `example/request_robot_state_example.cpp`

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
| `setLeftFlip()`                            | Requests a left flip.                                                                  |
| `setRightFlip()`                           | Requests a right flip.                                                                 |
| `setBipedal()`                             | Switches to bipedal gait.                                                              |
| `setInvBipedal()`                          | Switches to inverted bipedal gait.                                                     |
| `setPronk()`                               | Switches to Pronk jumping gait.                                                        |
| `setPace()`                                | Switches to Pace gait.                                                                 |
| `setBound()`                               | Switches to Bound gait.                                                                |
| `setVelocity(float x, float y, float yaw)` | Sends body velocity and yaw rate commands.                                             |
| `setStandUp()`                             | Requests stand mode.                                                                   |
| `setSitDown()`                             | Requests sit mode.                                                                     |
| `setDamping()`                             | Requests joint damping mode.                                                           |

Before calling `setZeroPositionsFlag()`, ensure the robot's feet, shanks, and the joints between shanks and thighs are all in contact with the ground.
`example/motion_level_control_example.cpp` sends a zeroing command at startup by default, so before running
`motion_level_control_example`, make sure the BPX is already in the required zeroing pose.

Example: `example/motion_level_control_example.cpp`

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
python3 scripts/build_wheels.py --cibuildwheel --out-dir wheelhouse
```

Running `cibuildwheel` requires Python 3.11 or newer as the host interpreter.

On local macOS machines, `cibuildwheel` only uses python.org CPython framework installs.
The build script skips configured CPython versions that are not installed locally. In
GitHub Actions, the full configured CPython matrix is built.

The GitHub Actions workflow in `.github/workflows/build-wheels.yml` builds wheel artifacts for Windows AMD64, Linux x86_64/aarch64, and macOS arm64. Run it manually from the Actions tab or push a `v*` tag. The generated wheels are uploaded as workflow artifacts and can be installed with:

To install from a local wheelhouse:

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

Python method names match the C++ SDK. Read APIs return a
`list`, `int`, or `float` on success, and `None` when no valid data has been received yet. Joint control APIs accept Python sequences with exactly 12 numbers:

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
