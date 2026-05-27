# BPX SDK Open

[English](README.md) | **中文**

`bpx_sdk_open` 提供一个轻量级 C++ SDK，用于读取 BPX 机器人状态，并发送运动级或关节级控制指令。

当前版本：`1.0.2`

SDK 提供三种使用模式：

| 模式 | 说明 |
| --- | --- |
| 状态查询层 | 只订阅并读取机器人状态，不发送控制指令，适合遥测和状态监控场景。 |
| 运控调用层 | 发送站立、坐下、阻尼、速度控制等高层运动控制指令，同时可读取机器人状态。 |
| 关节控制层 | 直接发送 12 自由度关节目标和力矩前馈指令，同时可读取普通状态和高频关节状态。 |

当前 SDK 接口和状态字段仍为初步实现，后续会逐步丰富可用接口和可读取的机器人状态。

## 目录结构

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

## 通用配置

公开配置定义在 `include/bpx_sdk_config.h` 中。

| 名称 | 含义 |
| --- | --- |
| `DEFAULT_SERVER_IP` | SDK 默认使用的机器人 IP。 |
| `DEFAULT_CLIENT_ROBOT_STATE_UDP_PORT` | 本机端口，用于接收机器人状态数据包。 |
| `DEFAULT_CLIENT_JOINT_STATE_UDP_PORT` | 本机端口，预留用于接收关节状态上传数据。 |

指令目标端口由机器人端固定，SDK 不将其作为公开配置暴露。

机器人无线连接 IP 为 `192.168.0.1`，有线连接 IP 为 `10.21.20.1`。SDK 默认机器人 IP 为
`10.21.20.1`。如需修改默认 IP，可以直接修改 `include/bpx_sdk_config.h` 中的配置，也可以在程序中调用
`setRobotIp` 接口设置目标机器人 IP。

## 运动状态与步态类型

头文件：`include/motion_types.h`

SDK 提供类型安全的运动状态和步态枚举，可用于读取当前/上一次运动状态和步态。

运动状态 `bpx_sdk::MotionState`：

| 枚举值 | 原始值 | 说明 |
| --- | --- | --- |
| `LyingDown` | `0` | 趴下状态。 |
| `StandingUp` | `1` | 起立状态。 |
| `Passive` | `2` | 被动状态。 |
| `SitDown` | `3` | 坐下状态。 |
| `Motion` | `6` | 运动状态。 |

步态 `bpx_sdk::MotionGait`：

| 枚举值 | 原始值 |
| --- | --- |
| `Walk` | `0` |
| `Bipedal` | `3` |
| `Flip` | `4` |
| `WalkPhase` | `6` |
| `PoseTracking` | `7` |
| `Running` | `8` |
| `WalkPeriod` | `10` |

## 状态查询层

头文件：`include/request_robot_state.h`

类：`bpx_sdk::RequestRobotState`

该层订阅机器人状态数据，并提供只读状态查询接口。当程序只需要遥测数据、不需要发送控制指令时使用该层。

状态查询层获得的关节数据和 IMU 数据频率较低，适合状态监控和低频遥测；如果需要高频关节状态，请使用关节控制层的 `HighRate` 接口。

连接与设置：

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

主要状态接口：

| 接口 | 说明 |
| --- | --- |
| `getJointPosition(float[12])` | 关节位置，单位为弧度。 |
| `getJointVelocity(float[12])` | 关节速度，单位为弧度/秒。 |
| `getJointTorque(float[12])` | 关节力矩。 |
| `getImuRpy(float[3])` | 机身横滚、俯仰、偏航角。 |
| `getImuQuat(float[4])` | 机身四元数。 |
| `getImuAcc(float[3])` | IMU 线加速度。 |
| `getImuOmega(float[3])` | IMU 角速度。 |
| `getCurrentVelocityBody(float[3])` | 当前机身坐标系速度。 |
| `getLegOdom(float[3])` | 腿部里程计数据。 |
| `getCurrentMotionState(uint8_t*)` | 当前运动状态。 |
| `getCurrentGait(uint8_t*)` | 当前步态。 |
| `getLastMotionState(uint8_t*)` | 上一次运动状态。 |
| `getLastGait(uint8_t*)` | 上一次步态。 |
| `getSubGait(uint8_t*)` | 当前子步态。 |
| `getCurrentMotionState(MotionState*)` | 当前运动状态，返回枚举类型。 |
| `getCurrentGait(MotionGait*)` | 当前步态，返回枚举类型。 |
| `getLastMotionState(MotionState*)` | 上一次运动状态，返回枚举类型。 |
| `getLastGait(MotionGait*)` | 上一次步态，返回枚举类型。 |
| `getMaxVelocity(float[3])` | 当前最大速度限制。 |
| `getBatteryLevel(uint8_t*)` | 电池电量百分比。 |
| `getBatteryCurrent(float*)` | 电池电流。 |
| `getMotorTemperature(float[12])` | 电机温度。 |
| `getDriverTemperature(float[12])` | 驱动器温度。 |
| `getJointStateTimestamp(uint32_t*)` | 最新关节状态时间戳。 |
| `getImuTimestamp(uint32_t*)` | 最新 IMU 数据时间戳。 |
| `getOdometryTimestamp(uint32_t*)` | 最新里程计数据时间戳。 |
| `getMotionStateTimestamp(uint32_t*)` | 最新运动状态时间戳。 |
| `getBatteryTimestamp(uint32_t*)` | 最新电池状态时间戳。 |

SDK 也提供返回数组或数值的可选辅助接口，例如 `getJointPositionArray()`、`getImuRpyArray()`、
`getSubGaitValue()` 和 `getBatteryLevelValue()`。这些接口返回 `std::optional`，适合调用方用更紧凑的方式判断读取是否成功。运动状态和步态也提供
`getCurrentMotionStateEnum()`、`getCurrentGaitEnum()`、`getLastMotionStateEnum()` 和 `getLastGaitEnum()`，
可直接返回 `MotionState` 或 `MotionGait`。

示例文件：`example/request_robot_state_example.cpp`

## 运控调用层

头文件：`include/motion_level_control.h`

类：`bpx_sdk::MotionLevelControl`

该层用于发送高层机器人控制指令。它继承自 `RequestRobotState`，因此同一个对象既可以发送运动指令，也可以读取机器人状态。

连接与设置：

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

控制接口：

| 接口 | 说明 |
| --- | --- |
| `setMotionCommandRate(uint16_t rate_hz)` | 设置周期性指令发送频率。 |
| `setVelocityControlFlag(bool enabled)` | 启用或关闭发送数据包中的速度控制标志。 |
| `setZeroPositionsFlag()` | 设置零位置标志，示例程序会在发送正式运动指令前先发送该标志。 |
| `setWalk()` | 切换到普通行走步态。 |
| `setRunning()` | 切换到跑步步态。 |
| `setLeftFlip()` | 请求左侧翻动作。 |
| `setRightFlip()` | 请求右侧翻动作。 |
| `setBipedal()` | 切换到双足正立步态。 |
| `setInvBipedal()` | 切换到双足倒立步态。 |
| `setPronk()` | 切换到 Pronk 跳跃步态。 |
| `setPace()` | 切换到 Pace 步态。 |
| `setBound()` | 切换到 Bound 步态。 |
| `setVelocity(float x, float y, float yaw)` | 发送机身速度/偏航速度指令。 |
| `setStandUp()` | 请求站立模式。 |
| `setSitDown()` | 请求坐下模式。 |
| `setDamping()` | 请求关节阻尼模式。 |
| `setUpright()` | 请求直立/等待模式。 |

使用 `setZeroPositionsFlag()` 标零前，必须确认机器人足底、小腿以及小腿和大腿连接处均接触地面。
`example/motion_level_control_example.cpp` 默认会在程序开始阶段发送标零指令，因此运行
`motion_level_control_example` 前，需要先确保 BPX 已处于上述标零姿态。

示例文件：`example/motion_level_control_example.cpp`

## 关节控制层

头文件：`include/joint_level_control.h`

类：`bpx_sdk::JointLevelControl`

该层用于直接发送关节控制指令。它同样继承 `RequestRobotState` 的全部状态查询接口，因此应用程序可以在同一个循环中发送关节目标并读取机器人状态。

建议关节级开发使用有线连接。

当机器人处于 `DevelopingState` 时，高频关节状态会通过关节状态通道上传。`JointLevelControl` 会在后台接收该数据流，并通过 `HighRate` 接口提供最新数据包。

关节控制模型为：

```text
torque = kp * (target_position - current_position)
       + kd * (target_velocity - current_velocity)
       + torque_feed_forward
```

连接与设置：

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

控制接口：

| 接口 | 说明 |
| --- | --- |
| `setJointCommand(kp, pos, kd, vel, tff)` | 发送完整的 12 自由度关节指令。 |
| `setJointKp(kp)` | 更新并发送关节 `kp`。 |
| `setJointPosition(pos)` | 更新并发送目标关节位置。 |
| `setJointKd(kd)` | 更新并发送关节 `kd`。 |
| `setJointVelocity(vel)` | 更新并发送目标关节速度。 |
| `setJointTorqueFeedForward(tff)` | 更新并发送前馈力矩。 |
| `setZeroJointCommand()` | 发送清零后的关节指令。 |

高频状态接口：

| 接口 | 说明 |
| --- | --- |
| `getJointPositionHighRate(float[12])` | 来自 `DevelopingState` 的最新高频关节位置。 |
| `getJointVelocityHighRate(float[12])` | 来自 `DevelopingState` 的最新高频关节速度。 |
| `getJointTorqueHighRate(float[12])` | 来自 `DevelopingState` 的最新高频关节力矩。 |
| `getImuRpyHighRate(float[3])` | 最新高频 IMU 横滚、俯仰、偏航角。 |
| `getImuQuatHighRate(float[4])` | 最新高频 IMU 四元数。 |
| `getImuAccHighRate(float[3])` | 最新高频 IMU 加速度。 |
| `getImuOmegaHighRate(float[3])` | 最新高频 IMU 角速度。 |
| `getJointStateTimestampHighRate(float*)` | 最新高频数据包携带的时间戳。 |
| `getJointStateSeqHighRate(uint32_t*)` | 最新高频数据包的序列号。 |

`example/joint_level_control_example.cpp` 演示了如何发送关节指令，并周期性打印运动状态、步态、电池电量、IMU RPY，以及前六个关节的高频位置、速度和力矩。

## 环境要求与安装步骤

环境要求：

- Ubuntu 22.04 及以上版本。

安装步骤：

在 `bpx_sdk_open` 目录内编译 SDK 示例：

```bash
mkdir build
cd build
cmake ..
make
```

编译完成后，生成的示例可执行文件位于 `build/`。

CMake 会根据当前系统架构自动链接对应的动态库。如有其他链接或构建需求，请修改 `CMakeLists.txt`。

## 运行说明

SDK 通过网络与 BPX 实现通讯。如果机器人 IP 与 `DEFAULT_SERVER_IP` 不同，请在连接前设置机器人 IP。运行程序前，建议先在开发主机上 `ping` BPX 的 IP，确认网络连通后再调用 `connect()`。
