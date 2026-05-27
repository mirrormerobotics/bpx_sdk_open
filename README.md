# BPX SDK Open

**English** | [中文](README.zh-CN.md)

`bpx_sdk_open` provides a lightweight C++ SDK for reading BPX robot state and sending motion-level or joint-level control commands.

Current version: `1.0.2`

The SDK offers three usage modes:

| Mode | Description |
| --- | --- |
| State Query Layer | Subscribes to and reads robot state only; does not send control commands. Suitable for telemetry and state monitoring. |
| Motion Control Layer | Sends high-level motion commands such as stand, sit, damping, and velocity control, while also reading robot state. |
| Joint Control Layer | Sends 12-DOF joint target and torque feedforward commands directly, while reading both standard state and high-rate joint state. |

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
  example/
    request_robot_state_example.cpp
    motion_level_control_example.cpp
    joint_level_control_example.cpp
```

## General Configuration

Public configuration is defined in `include/bpx_sdk_config.h`.

| Name | Description |
| --- | --- |
| `DEFAULT_SERVER_IP` | Default robot IP used by the SDK. |
| `DEFAULT_CLIENT_ROBOT_STATE_UDP_PORT` | Local port for receiving robot state packets. |
| `DEFAULT_CLIENT_JOINT_STATE_UDP_PORT` | Local port reserved for receiving joint state upload data. |

Command destination ports are fixed on the robot side and are not exposed as public SDK configuration.

The robot's wireless IP is `192.168.0.1`, and the wired IP is `10.21.20.1`. The SDK default robot IP is
`10.21.20.1`. To change the default IP, edit the configuration in `include/bpx_sdk_config.h`, or call the
`setRobotIp` API in your program to set the target robot IP.

## Motion State and Gait Types

Header: `include/motion_types.h`

The SDK provides type-safe motion state and gait enumerations for reading the current and previous motion state and gait.

Motion state `bpx_sdk::MotionState`:

| Enum Value | Raw Value | Description |
| --- | --- | --- |
| `LyingDown` | `0` | Lying down. |
| `StandingUp` | `1` | Standing up. |
| `Passive` | `2` | Passive mode. |
| `SitDown` | `3` | Sitting down. |
| `Motion` | `6` | In motion. |

Gait `bpx_sdk::MotionGait`:

| Enum Value | Raw Value |
| --- | --- |
| `Walk` | `0` |
| `Bipedal` | `3` |
| `Flip` | `4` |
| `WalkPhase` | `6` |
| `PoseTracking` | `7` |
| `Running` | `8` |
| `WalkPeriod` | `10` |

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

| API | Description |
| --- | --- |
| `getJointPosition(float[12])` | Joint positions in radians. |
| `getJointVelocity(float[12])` | Joint velocities in rad/s. |
| `getJointTorque(float[12])` | Joint torques. |
| `getImuRpy(float[3])` | Body roll, pitch, and yaw. |
| `getImuQuat(float[4])` | Body orientation quaternion. |
| `getImuAcc(float[3])` | IMU linear acceleration. |
| `getImuOmega(float[3])` | IMU angular velocity. |
| `getCurrentVelocityBody(float[3])` | Current body-frame velocity. |
| `getLegOdom(float[3])` | Leg odometry data. |
| `getCurrentMotionState(uint8_t*)` | Current motion state. |
| `getCurrentGait(uint8_t*)` | Current gait. |
| `getLastMotionState(uint8_t*)` | Previous motion state. |
| `getLastGait(uint8_t*)` | Previous gait. |
| `getSubGait(uint8_t*)` | Current sub-gait. |
| `getCurrentMotionState(MotionState*)` | Current motion state as an enum. |
| `getCurrentGait(MotionGait*)` | Current gait as an enum. |
| `getLastMotionState(MotionState*)` | Previous motion state as an enum. |
| `getLastGait(MotionGait*)` | Previous gait as an enum. |
| `getMaxVelocity(float[3])` | Current maximum velocity limits. |
| `getBatteryLevel(uint8_t*)` | Battery level as a percentage. |
| `getBatteryCurrent(float*)` | Battery current. |
| `getMotorTemperature(float[12])` | Motor temperatures. |
| `getDriverTemperature(float[12])` | Driver temperatures. |
| `getJointStateTimestamp(uint32_t*)` | Latest joint state timestamp. |
| `getImuTimestamp(uint32_t*)` | Latest IMU data timestamp. |
| `getOdometryTimestamp(uint32_t*)` | Latest odometry timestamp. |
| `getMotionStateTimestamp(uint32_t*)` | Latest motion state timestamp. |
| `getBatteryTimestamp(uint32_t*)` | Latest battery state timestamp. |

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

| API | Description |
| --- | --- |
| `setMotionCommandRate(uint16_t rate_hz)` | Sets the periodic command send rate. |
| `setVelocityControlFlag(bool enabled)` | Enables or disables the velocity control flag in outgoing packets. |
| `setZeroPositionsFlag()` | Sets the zero-position flag. Example programs send this before formal motion commands. |
| `setWalk()` | Switches to normal walking gait. |
| `setRunning()` | Switches to running gait. |
| `setLeftFlip()` | Requests a left flip. |
| `setRightFlip()` | Requests a right flip. |
| `setBipedal()` | Switches to bipedal gait. |
| `setInvBipedal()` | Switches to inverted bipedal gait. |
| `setPronk()` | Switches to Pronk jumping gait. |
| `setPace()` | Switches to Pace gait. |
| `setBound()` | Switches to Bound gait. |
| `setVelocity(float x, float y, float yaw)` | Sends body velocity and yaw rate commands. |
| `setStandUp()` | Requests stand mode. |
| `setSitDown()` | Requests sit mode. |
| `setDamping()` | Requests joint damping mode. |

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

| API | Description |
| --- | --- |
| `setJointCommand(kp, pos, kd, vel, tff)` | Sends a full 12-DOF joint command. |
| `setJointKp(kp)` | Updates and sends joint `kp`. |
| `setJointPosition(pos)` | Updates and sends target joint positions. |
| `setJointKd(kd)` | Updates and sends joint `kd`. |
| `setJointVelocity(vel)` | Updates and sends target joint velocities. |
| `setJointTorqueFeedForward(tff)` | Updates and sends feedforward torques. |
| `setZeroJointCommand()` | Sends a zeroed joint command. |

High-rate state APIs:

| API | Description |
| --- | --- |
| `getJointPositionHighRate(float[12])` | Latest high-rate joint positions from `DevelopingState`. |
| `getJointVelocityHighRate(float[12])` | Latest high-rate joint velocities from `DevelopingState`. |
| `getJointTorqueHighRate(float[12])` | Latest high-rate joint torques from `DevelopingState`. |
| `getImuRpyHighRate(float[3])` | Latest high-rate IMU roll, pitch, and yaw. |
| `getImuQuatHighRate(float[4])` | Latest high-rate IMU quaternion. |
| `getImuAccHighRate(float[3])` | Latest high-rate IMU acceleration. |
| `getImuOmegaHighRate(float[3])` | Latest high-rate IMU angular velocity. |
| `getJointStateTimestampHighRate(float*)` | Timestamp carried by the latest high-rate packet. |
| `getJointStateSeqHighRate(uint32_t*)` | Sequence number of the latest high-rate packet. |

`example/joint_level_control_example.cpp` demonstrates how to send joint commands and periodically print motion state, gait, battery level, IMU RPY, and high-rate position, velocity, and torque for the first six joints.

## Requirements and Installation

Requirements:

- Ubuntu 22.04 or later.

Installation:

Build the SDK examples inside the `bpx_sdk_open` directory:

```bash
mkdir build
cd build
cmake ..
make
```

After building, the example executables are located in `build/`.

CMake automatically links the shared library for the current system architecture. Modify `CMakeLists.txt` if you have other linking or build requirements.

## Running

The SDK communicates with the BPX over the network. If the robot IP differs from `DEFAULT_SERVER_IP`, set the robot IP before connecting. Before running your program, `ping` the BPX IP from the development host to verify network connectivity, then call `connect()`.
