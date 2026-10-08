# BPX SDK Open

[English](README.md) | **中文**

`bpx_sdk_open` 提供一个轻量级 C++ SDK，用于读取 BPX 机器人状态，并发送运动级或关节级控制指令。

SDK 版本：`1.0.10`

### 1.0.10 预发布（rc.1）

当前分支提供 **v1.0.10-rc.1 预发布版**，用于试用验证。原生 SDK、Python 包和
wheel 版本仍为 `1.0.10`，候选版编号由 GitHub 标签和 Release 标识；稳定分支
`master` 继续保留 1.0.9。

从[预发布页面](https://github.com/mirrormerobotics/bpx_sdk_open/releases/tag/v1.0.10-rc.1)
下载与操作系统、架构和 Python 版本匹配的 wheel。附件包括 CPython 3.8–3.14 在
Linux x86_64/aarch64、Windows AMD64 和 macOS arm64 上的 28 个 wheel，以及
用于校验的 `SHA256SUMS`。

```bash
python3 -m pip install --no-index --find-links /path/to/downloaded --force-reinstall bpx-sdk-open==1.0.10
python3 -c "import bpx_sdk; print(bpx_sdk.__version__)"
```

请将 `/path/to/downloaded` 替换为存放已下载 wheel 的目录。替换另一个同为 `1.0.10` 的 wheel
时使用 `--force-reinstall`。构建 C++ 示例或从源码安装前，请先检出标签
`v1.0.10-rc.1`。硬急停状态和活动故障通道需要配套机器人固件；收到对应反馈前，
查询返回 `None`/false，旧固件未发布这些字段时同样如此。本次预发布通过 GitHub 分发。

如需远程构建，执行 `python3 scripts/build_wheels_github.py --ref prerelease/1.0.10`。
脚本默认从当前仓库的 `origin` 自动识别目标，下载完成后默认不查询账单；维护者可
按需添加 `--show-usage`，或使用 `--usage-only` 单独查询。

文档更新日期：`2026-10-08`

SDK 提供三种使用模式：

| 模式    | 说明                                       |
| ----- | ---------------------------------------- |
| 状态查询层 | 只订阅并读取机器人状态，不发送控制指令，适合遥测和状态监控场景。         |
| 运控调用层 | 发送站立、坐下、阻尼、速度控制等高层运动控制指令，同时可读取机器人状态。     |
| 关节控制层 | 直接发送 12 自由度关节目标和力矩前馈指令，同时可读取普通状态和高频关节状态。 |

当前 SDK 接口和状态字段仍为初步实现，后续会逐步丰富可用接口和可读取的机器人状态。

## 使用须知

使用前请确认 SDK 与机器人固件版本匹配。

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

## 通用配置

公开配置定义在 `include/bpx_sdk_config.h` 中。

| 名称                                    | 含义                   |
| ------------------------------------- | -------------------- |
| `DEFAULT_SERVER_IP`                   | SDK 默认使用的机器人 IP。     |
| `DEFAULT_CLIENT_ROBOT_STATE_UDP_PORT` | 本机端口，用于接收机器人状态数据包。   |
| `DEFAULT_CLIENT_JOINT_STATE_UDP_PORT` | 本机端口，预留用于接收关节状态上传数据。 |

指令目标端口由机器人端固定，SDK 不将其作为公开配置暴露。

SDK 默认机器人 IP 为 `10.21.20.1`（对应 RJ45 网口直连）。如需修改默认 IP，可以直接修改
`include/bpx_sdk_config.h` 中的配置，也可以在程序中调用 `setRobotIp` 接口设置目标机器人 IP。不同网络连接方式下应使用的 IP 见下文「网络连接与 IP」。

## 网络连接与 IP

SDK 通过以太网或 WiFi 与 BPX 通信。请根据实际连接方式选择对应的机器人 IP，并在连接前通过 `setRobotIp` 或
`DEFAULT_SERVER_IP` 进行配置：

| 连接方式         | 说明                                            | 机器人 IP               |
| ------------ | --------------------------------------------- | -------------------- |
| RJ45 网口直连    | 开发主机通过 RJ45 网线与机器人直连                          | `10.21.20.1`         |
| 机器人 AP 热点    | 开发主机连接机器人开启的 AP 热点                            | `10.21.40.1`         |
| Station WiFi | 机器人已配置 Station 模式并接入 WiFi，且开发主机与机器人在同一 WiFi 下 | 使用机器人连上 WiFi 后获得的 IP |

## 运动状态与步态类型

头文件：`include/motion_types.h`

SDK 提供类型安全的运动状态和步态枚举，可用于读取当前/上一次运动状态和步态。

运动状态 `bpx_sdk::MotionState`：

| 枚举值          | 原始值 | 说明    |
| ------------ | --- | ----- |
| `LyingDown`  | `0` | 趴下状态。 |
| `StandingUp` | `1` | 起立状态。 |
| `Passive`    | `2` | 被动状态。 |
| `SitDown`    | `3` | 趴下中状态。 |
| `Motion`     | `6` | 运动状态。 |
| `ChargingSitDown` | `24` | 充电坐下动作执行中。 |

步态 `bpx_sdk::MotionGait`：

| 枚举值            | 原始值 |
| -------------- | --- |
| `Walk`         | `0` |
| `Bipedal`      | `3` |
| `Flip`         | `4` |
| `WalkPhase`    | `6` |
| `PoseTracking` | `7` |
| `Running`      | `8` |
| `Jump` | `12` |

### 主步态与子步态

部分主步态需要结合 `getSubGait()` 返回的子步态值，才能确定机器人当前执行的具体动作：

| 主步态            | 主步态原始值 | 子步态          | 子步态原始值 | 说明   |
| -------------- | ------ | ------------ | ------- | ---- |
| `Walk`         | `0`    | —            | —       | 行走   |
| `Bipedal`      | `3`    | `F_HANDSTAND` | `-1`    | 倒立   |
| `Bipedal`      | `3`    | `B_HANDSTAND` | `1`     | 正立   |
| `Flip`         | `4`    | `GAIT_L_FLIP` | `-1`    | 左侧翻  |
| `Flip`         | `4`    | `GAIT_R_FLIP` | `-2`    | 右侧翻  |
| `WalkPhase`    | `6`    | `Pace`        | `2`     | 遛步   |
| `WalkPhase`    | `6`    | `Bound`       | `1`     | 跳跃   |
| `WalkPhase`    | `6`    | `Pronk`       | `-1`    | 直腿跳  |
| `PoseTracking` | `7`    | —            | —       | 原地扭动 |
| `Running`      | `8`    | `Run`         | `0`     | 奔跑   |
| `Jump` | `12` | `UpJump` | `0` | 向上跳 |
| `Jump` | `12` | `FrontJump` | `1` | 向前跳 |
| `Jump` | `12` | `BackJump` | `2` | 向后跳 |
| `Jump` | `12` | `LeftJump` | `-1` | 向左跳 |
| `Jump` | `12` | `RightJump` | `-2` | 向右跳 |

表中的“—”表示该主步态不需要通过子步态进一步区分。`getSubGait(uint8_t*)` 和
`getSubGaitValue()` 返回的是 `uint8_t`；子步态原始值为负数时，应按 `int8_t` 解释，例如
`-1` 和 `-2` 在线上传输后分别对应 `255` 和 `254`：

```cpp
uint8_t raw_sub_gait = 0;
if (robot_state.getSubGait(&raw_sub_gait)) {
    const int8_t sub_gait = static_cast<int8_t>(raw_sub_gait);
}
```

腿部里程计 `bpx_sdk::LegOdom`：

| 字段                 | 长度  | 说明                         |
| ------------------ | --- | -------------------------- |
| `velocity_body`    | `3` | 机身坐标系线速度，单位 m/s。          |
| `position`         | `3` | 世界坐标系位置，单位 m。             |
| `orientation`      | `4` | 世界坐标系姿态四元数，顺序为 x/y/z/w。   |
| `angular_velocity` | `3` | 机身坐标系角速度，单位 rad/s。        |

### 步态速度限制

速度指令使用 `[x, y, yaw]`，其中 `x`、`y` 为机身坐标系线速度，单位 m/s；`yaw` 为偏航角速度，单位 rad/s。以下数值为控制器内部对各步态速度指令的限幅：

| 步态             | 速度限制 `[x, y, yaw]`                                   |
| -------------- | ---------------------------------------------------- |
| `Walk`         | `[1.5, 0.5, 2.0]`                                    |
| `Bipedal`      | `[0.8, 0.5, 1.5]`                                    |
| `Flip`         | `[0.0, 0.0, 0.0]`                                    |
| `WalkPhase`    | `[1.5, 1.0, 2.0]`                                    |
| `PoseTracking` | `[0.0, 0.0, 0.0]`                                    |
| `Running`      | `x: [-1.5, 3.0]`，`y: [-1.0, 1.0]`，`yaw: [-2.0, 2.0]` |

`Flip` 不使用行走速度指令。`PoseTracking` 限制的是原地扭动指令，不是机身移动速度。对于 `Walk`、`Bipedal`、`WalkPhase` 和 `Running`，横向速度和偏航角速度会随着 `abs(x)` 增大进一步收缩，因此表中列出的是各轴基础限幅。

#### `Walk` 步态起步阈值

通过 `MotionLevelControl::setVelocity(x, y, yaw)` 发送绝对速度值前，必须先调用
`setVelocityControlFlag(true)`。`setVelocity()` 不会自动开启该模式；未开启时，传入的三个参数不会被按绝对速度值使用。

```cpp
motion.setVelocityControlFlag(true);
motion.setVelocity(0.3f, 0.0f, 0.0f);
```

`Walk` 控制器会先对速度指令进行限幅、轴间约束和平滑处理，然后对处理后的速度
`[vx, vy, yaw]` 进行以下判断：

```text
sqrt(vx² + vy² + yaw²) < 0.2
```

满足该条件时，控制器会将周期步态相位输入置零，机器人通常保持站立而不会起步。这不是 SDK 丢弃指令或将速度直接清零，而是 `Walk` 策略的低速静止处理。

当指令未触发限幅且速度平滑过程已稳定时，上述判断可近似理解为直接作用于 `setVelocity(vx, vy, yaw)` 的三个参数：

- 仅使用单轴指令时，`vx`、`vy` 或 `yaw` 的理论起步边界均为数值 `0.2`。`vx`、`vy` 的单位为 m/s，`yaw` 的单位为 rad/s。
- 这是三轴合成阈值，不是要求每个轴都达到 `0.2`。例如 `[0.15, 0.15, 0.0]` 的计算结果约为 `0.212`，已超过阈值；`[0.1, 0.1, 0.1]` 的计算结果约为 `0.173`，仍处于低速静止范围。
- 判断条件是严格的小于 `0.2`，但不建议依赖恰好 `0.2` 的边界值。单轴起步测试建议使用绝对值 `0.25`～`0.3`。
- 由于存在速度平滑，发送指令后的前几个控制周期中，处理后的速度可能仍小于 `0.2`，起步会有短暂延迟。

注意：上述公式按数值直接合成单位为 m/s 的线速度与单位为 rad/s 的角速度，它是当前控制器的阈值规则，不是具有统一物理单位的速度模长。

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

`setRobotStateUploadRate()` 设置机器人端向 SDK 客户端发送普通 UDP 状态的请求频率上限，不是 SDK 向机器人发送运动指令的频率。请在 `connect()` 前调用；合法范围为 1--200 Hz，默认 100 Hz，非法值由机器人端回退为 100 Hz。连接期间修改只在断开并重新连接后生效。

`connect()` 表示启动“期望连接”：机器人暂时不可达时 SDK 会以100 ms至2 s的有界退避在后台自动重连，实时 TCP 状态请用 `isConnected()` 判断。只有收到机器人首个完整订阅响应才会报告已连接；即使断 Wi-Fi、断电或整机硬重启没有产生 TCP FIN/RST，持续收不到响应也会在有界时间内主动废弃半开连接并进入重连。显式 `disconnect()`、对象析构或进程退出会停止重试。每次真实连接及后台重连成功后，SDK 都会在当前 TCP 会话自动发起一次授时；重连仍不会恢复断线前的非零运动或关节目标。

运动控制的新 TCP 会话会先周期发送完整零命令；零基线完成后，业务代码必须显式调用新的运动控制 API 才能恢复非零目标。关节控制重连后不会重放旧帧，必须先调用 `setJointCommand()` 提交新的完整关节命令（或调用 `setZeroJointCommand()`）；在此之前局部关节 setter 返回 `false`。

普通状态按数据组保留原生上限，因此每组实际频率为 `min(请求频率, 原生上限)`：关节 1000 Hz、IMU 200 Hz、腿部里程计 50 Hz、运动状态 10 Hz、电池与温度 1 Hz。关节控制层的独立 `HighRate` 状态通道不受该接口影响。命令行示例可使用 `--state-rate 50` 设置同一参数。

### 连接后自动同步机器人时间

用户只需正常调用 `connect()`。SDK 在每次收到首个完整订阅响应、确认真实连接成功时，将开发机当前 epoch 通过同一 TCP 会话自动发送给机器人；后台自动重连建立的新会话也会再次发送。该请求不会改变当前控制模式，失败也不会把已建立的控制连接改判为失败。

机器人使用单一10分钟绝对偏差阈值：偏差不大于10分钟时不拨钟，超过10分钟时采用SDK时间，并同时更新Linux `CLOCK_REALTIME` 和使用UTC的硬件RTC。NTP状态只用于响应和诊断，不再阻止超过10分钟的SDK授时。阈值跳过、权限错误和RTC部分失败均由机器人返回并记录。SDK 不再提供显式授时接口，普通用户只需要调用 `connect()`。

机器人随包配置在受信网络中默认启用自动授时。现有协议没有身份认证，非受信网络必须在机器人配置中关闭 `sdk_time_sync_enabled`。错误码 `9` 表示 NTP 状态未知，`10` 表示 RTC 写入或回读失败；第二版协议会明确拒绝第一版客户端，因为旧响应无法安全表达策略跳过和 RTC 部分应用。

自动授时需要所用平台的 SDK 版本支持。

自动授时连接示例：
控制超时和仲裁继续使用单调时钟；该接口不会同步 MCU、IMU 或关节设备时钟，也不能替代 NTP/chrony 持续授时。

主要状态接口：

| 接口                                    | 说明              |
| ------------------------------------- | --------------- |
| `getJointPosition(float[12])`         | 关节位置，单位为弧度。     |
| `getJointVelocity(float[12])`         | 关节速度，单位为弧度/秒。   |
| `getJointTorque(float[12])`           | 关节力矩。           |
| `getImuRpy(float[3])`                 | 机身横滚、俯仰、偏航角。    |
| `getImuQuat(float[4])`                | 机身四元数。          |
| `getImuAcc(float[3])`                 | IMU 线加速度。       |
| `getImuOmega(float[3])`               | IMU 角速度。        |
| `getLegOdom(LegOdom*)`                | 腿部里程计数据。        |
| `getCurrentMotionState(uint8_t*)`     | 当前运动状态。         |
| `getCurrentGait(uint8_t*)`            | 当前步态。           |
| `getLastMotionState(uint8_t*)`        | 上一次运动状态。        |
| `getLastGait(uint8_t*)`               | 上一次步态。          |
| `getSubGait(uint8_t*)`                | 当前子步态。          |
| `getCurrentMotionState(MotionState*)` | 当前运动状态，返回枚举类型。  |
| `getCurrentGait(MotionGait*)`         | 当前步态，返回枚举类型。    |
| `getLastMotionState(MotionState*)`    | 上一次运动状态，返回枚举类型。 |
| `getLastGait(MotionGait*)`            | 上一次步态，返回枚举类型。   |
| `getMaxVelocity(float[3])`            | 当前最大速度限制。       |
| `getBatteryLevel(uint8_t*)`           | 电池电量百分比。        |
| `getBatteryCurrent(float*)`           | 电池电流。           |
| `getMotorTemperature(float[12])`      | 电机温度。           |
| `getDriverTemperature(float[12])`     | 驱动器温度。          |
| `getJointStateTimestamp(uint32_t*)`   | 最新关节状态时间戳。      |
| `getImuTimestamp(uint32_t*)`          | 最新 IMU 数据时间戳。   |
| `getOdometryTimestamp(uint32_t*)`     | 最新里程计数据时间戳。     |
| `getMotionStateTimestamp(uint32_t*)`  | 最新运动状态时间戳。      |
| `getBatteryTimestamp(uint32_t*)`      | 最新电池状态时间戳。      |

SDK 也提供返回数组、结构体或数值的可选辅助接口，例如 `getJointPositionArray()`、`getImuRpyArray()`、
`getLegOdomValue()`、
`getSubGaitValue()` 和 `getBatteryLevelValue()`。这些接口返回 `std::optional`，适合调用方用更紧凑的方式判断读取是否成功。运动状态和步态也提供
`getCurrentMotionStateEnum()`、`getCurrentGaitEnum()`、`getLastMotionStateEnum()` 和 `getLastGaitEnum()`，
可直接返回 `MotionState` 或 `MotionGait`。

示例文件：`example/request_robot_state_example.cpp`

### 整机 SN 与机型（C++）

`RequestRobotState` 及其派生类 `MotionLevelControl`、`JointLevelControl` 提供：

| 接口 | 说明 |
| --- | --- |
| `getRobotSerialNumber(std::string*)` | 读取整机原始 SN，成功返回 `true`。 |
| `getRobotModel(RobotModel*)` | 读取机器人硬件识别出的机型。 |
| `getRobotSerialNumberValue()` | 返回 `std::optional<std::string>`。 |
| `getRobotModelValue()` | 返回 `std::optional<RobotModel>`。 |
| `getControlMode(ControlMode*)` | 读取当前控制模式，成功返回 `true`。 |
| `getControlModeValue()` | 返回 `std::optional<ControlMode>`。 |

`RobotModel` 定义于 `include/motion_types.h`：

| 枚举 | SDK 枚举值 |
| --- | --- |
| `Unknown` | `0` |
| `BPX` | `1` |
| `BPXPro` | `2` |
| `BPW` | `3` |

SN 为整机序列号。

```cpp
// 在 connect() 后的状态读取循环中调用。
auto sn = robot_state.getRobotSerialNumberValue();
auto model = robot_state.getRobotModelValue();
if (sn && model) {
    // *sn 为空表示机器人未读到 SN；*model 为识别出的机型。
}
```

`connect()` 返回前会尝试同步获取身份信息；查询失败或固件不支持时返回 `false` / `std::nullopt`。
空 SN 表示序列号不可用，`Unknown` 表示机型未知。
连接初始化和自动重连时，成功获取身份信息后打印一次：

```text
robot identity: SN=BPX-example-SN, model=BPX-Pro
```

当前控制模式随状态反馈更新，可通过 `getControlMode()` 或 `getControlModeValue()` 读取。
`ControlMode` 取值为 `Unknown=0`、`RemoteControl=1`（APP/遥控器）、`Navigator=2`（导航）。
未收到有效反馈或断连时返回 `false` / `std::nullopt`。

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

| 接口                                         | 说明                             |
| ------------------------------------------ | ------------------------------ |
| `setMotionCommandRate(uint16_t rate_hz)`   | 设置周期性指令发送频率。                   |
| `setVelocityControlFlag(bool enabled)`     | 启用或关闭发送数据包中的速度控制标志。            |
| `setZeroPositionsFlag()`                   | 设置零位置标志，示例程序会在发送正式运动指令前先发送该标志。 |
| `setWalk()`                                | 切换到普通行走步态。                     |
| `setRunning()`                             | 切换到跑步步态。                       |
| `setLeftFlip()`                            | 请求左侧翻动作。                       |
| `setRightFlip()`                           | 请求右侧翻动作。                       |
| `setBipedal()`                             | 切换到双足正立步态。                     |
| `setInvBipedal()`                          | 切换到双足倒立步态。                     |
| `setUpJump()` | 请求 BPX 向上跳。 |
| `setFrontJump()` | 请求 BPX 向前跳。 |
| `setBackJump()` | 请求 BPX 向后跳。 |
| `setLeftJump()` | 请求 BPX 向左跳。 |
| `setRightJump()` | 请求 BPX 向右跳。 |
| `setPronk()`                               | 切换到 Pronk 跳跃步态。                |
| `setPace()`                                | 切换到 Pace 步态。                   |
| `setBound()`                               | 切换到 Bound 步态。                  |
| `setVelocity(float x, float y, float yaw)` | 发送机身速度/偏航速度指令。                 |
| `setStandUp()`                             | 请求站立模式。                        |
| `setSitDown()`                             | 请求坐下模式。                        |
| `setChargingSitDown()` | 请求充电坐下；返回命令是否发送成功。 |
| `setDamping()`                             | 请求关节阻尼模式。                      |

连接就绪且机器人处于运动模式后，调用 `setChargingSitDown()` 请求充电坐下。
它发送目标状态 `24`、Walk 步态和零速度；机器端仍执行静止时间及状态切换检查。
返回 `true` 仅表示发送成功，不代表动作完成。执行中，`getCurrentMotionState()`
返回 `MotionState::ChargingSitDown`；进入阻尼后，当前状态为 `Passive`，
`getLastMotionState()` 返回 `ChargingSitDown`。这些字段沿用最高 10 Hz 的运动状态反馈。
上一状态不是完成标志，阻尼请求或安全保护也可能提前中断动作；充电器是否插入需另读
`getChargerIn1/2()`，硬急停开关状态见 `getHardEstop()`。

五种定向跳跃适用于 BPX。连接就绪后，在机器人进入运动模式且速度指令为零时调用。
每次调用请求一次跳跃。`setPronk()` 用于切换 Pronk 步态。
`setBipedal()` / `setInvBipedal()` 适用于 BPX，BPX-Pro 不支持正立和倒立。


使用 `setZeroPositionsFlag()` 标零前，必须确认机器人足底、小腿以及小腿和大腿连接处均接触地面。
`example/motion_level_control_example.cpp` 默认会在程序开始阶段发送标零指令，因此运行
`motion_level_control_example` 前，需要先确保 BPX 已处于上述标零姿态。

示例文件：`example/motion_level_control_example.cpp`

### 原地模式（PoseTracking）

BPX 和 BPX-Pro 支持通过 `setPoseTracking()` 请求原地模式。机器人须已处于 `MotionState::Motion`；请求会将六轴全部清零，以满足步态切换条件。返回 `true` 只表示命令已发送，需要等待 `getCurrentGait()` / `getCurrentGaitEnum()` 返回 `PoseTracking` 后，再发送非零姿态输入。

`setPose(x, y, z, r)` 在 C++ 和 Python 中都接收四个有限的归一化值，每个值须在 `[-1, 1]` 内，`r` 对应 yaw 输入。这些不是以米或弧度表示的位姿。SDK 内部按 `[x, y, r, z, 0, 0]` 填充协议通道，未开放的两个通道固定为零。

当前控制端在各轴缩放前映射为 `[-y, x, r, 0, 0, z]`，保留已有的 X/Y 互换及互换后的 X 反向；SDK 不重复执行该映射。

- `setPose()` 自动使用归一化控制，不需要调用 `setVelocityControlFlag(false)`，也不会改变调用者为普通速度控制设置的标志。
- 未在当前连接中调用 `setPoseTracking()`、已选择其他步态、连接未就绪或数值非法时，`setPose()` 返回 `false`，保留上一条有效命令。Python 的参数数量或类型错误抛出 `TypeError`。
- `setPose()` 更新后由 SDK 后台周期发送，直到后续命令覆盖；它不会自动等待步态反馈。使用 `setPose()` 控制原地姿态，使用 `setVelocity()` 控制行走速度。
- `setPose(0, 0, 0, 0)` 回到中性输入。`setWalk()` 等步态选择接口离开原地模式时会清零六轴，并恢复原来的速度控制标志；下一条非零行走命令应在步态反馈确认后发送。
- 断线会清零全部六轴；重连后须重新选择原地模式。

完整示例：[C++](example/pose_tracking_example.cpp)、[Python](example/pose_tracking_example.py)。随包提供的 Linux x86_64/aarch64、Windows x86_64 和 macOS arm64 库均包含这两个新接口。

两个示例复用公共命令行参数，支持 `--robot-ip`、`--state-port`（别名 `--robot-state-port`）、`--tcp-local-port`、`--state-rate` 和 `-h/--help`，默认值与其他运动控制示例一致：

示例连接后自动调用 `setStandUp()`，等待状态反馈为 `Motion` 后再进入原地模式，依次测试 `x → y → z → r`。每个通道先在 `-1000` 保持 1 秒，再用 3 秒线性增加到 `1000`，每 20 ms 更新一次（共 151 个值），其余通道始终为零。发送前除以 `1000` 转为 SDK 的归一化输入，终端打印原始 `x/y/z/r` 值；`r` 对应 SDK 的 `yaw` 通道。每个通道到达 `1000` 后保持 1 秒，再归零并等待 1 秒，随后测试下一个通道。最后一个通道也归零等待 1 秒，再切回 `Walk`，随后调用 `setSitDown()` 自动趴下，等待状态反馈为 `LyingDown` 后断开连接并退出。站立和趴下的完成等待各最多 10 秒，超时或发送失败时停止流程并断开连接。

```bash
./build/pose_tracking_example --robot-ip <机器人IP> --state-rate 100
python3 example/pose_tracking_example.py --robot-ip <机器人IP> --state-rate 100
```

```cpp
// 在连接就绪、机器人处于 Motion 状态后请求。
const bool sent = motion.setPoseTracking();
// 在后续状态循环确认进入原地模式，再发送姿态。
if (sent && motion.getCurrentGaitEnum() == bpx_sdk::MotionGait::PoseTracking) {
    motion.setPose(0.1f, -0.1f, 0.0f, 0.0f);
}
```

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

| 接口                                       | 说明                |
| ---------------------------------------- | ----------------- |
| `setJointCommand(kp, pos, kd, vel, tff)` | 发送完整的 12 自由度关节指令。 |
| `setJointKp(kp)`                         | 更新并发送关节 `kp`。     |
| `setJointPosition(pos)`                  | 更新并发送目标关节位置。      |
| `setJointKd(kd)`                         | 更新并发送关节 `kd`。     |
| `setJointVelocity(vel)`                  | 更新并发送目标关节速度。      |
| `setJointTorqueFeedForward(tff)`         | 更新并发送前馈力矩。        |
| `setZeroJointCommand()`                  | 发送清零后的关节指令。       |

高频状态接口：

| 接口                                       | 说明                              |
| ---------------------------------------- | ------------------------------- |
| `getJointPositionHighRate(float[12])`    | 来自 `DevelopingState` 的最新高频关节位置。 |
| `getJointVelocityHighRate(float[12])`    | 来自 `DevelopingState` 的最新高频关节速度。 |
| `getJointTorqueHighRate(float[12])`      | 来自 `DevelopingState` 的最新高频关节力矩。 |
| `getImuRpyHighRate(float[3])`            | 最新高频 IMU 横滚、俯仰、偏航角。             |
| `getImuQuatHighRate(float[4])`           | 最新高频 IMU 四元数。                   |
| `getImuAccHighRate(float[3])`            | 最新高频 IMU 加速度。                   |
| `getImuOmegaHighRate(float[3])`          | 最新高频 IMU 角速度。                   |
| `getJointStateTimestampHighRate(float*)` | 最新高频数据包携带的时间戳。                  |
| `getJointStateSeqHighRate(uint32_t*)`    | 最新高频数据包的序列号。                    |

`example/joint_level_control_example.cpp` 演示了如何发送关节指令，并周期性打印运动状态、步态、电池电量、IMU RPY，以及前六个关节的高频位置、速度和力矩。

## 环境要求与安装步骤

环境要求：

- Linux 下需要 Ubuntu 22.04 及以上版本。
- macOS 下使用当前随包提供的 aarch64 SDK 二进制。
- Windows 下使用当前随包提供的 x86_64 SDK 二进制。
- Python 3.8 及以上版本（使用 Python 封装时需要）。Windows 下需要 64 位 Python。

安装步骤：

在 `bpx_sdk_open` 目录内编译 SDK 示例：

```bash
mkdir build
cd build
cmake ..
make
```

Windows 下可以打开 x64 Native Tools Command Prompt for VS，并在 `bpx_sdk_open` 目录内执行：

```bat
cmake -S . -B build\windows_x64_examples -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build\windows_x64_examples
```

编译完成后，生成的示例可执行文件位于 `build/`。

CMake 会根据当前系统架构自动链接对应的动态库。如有其他链接或构建需求，请修改 `CMakeLists.txt`。

### Python 跳跃与状态查询

`RequestRobotState`、`MotionLevelControl` 和 `JointLevelControl` 均提供
`getRobotSerialNumber()` 和 `getRobotModel()`：前者返回字符串，后者返回机型整数；
未获取到身份信息时返回 `None`。空字符串表示 SN 不可用。

```python
# 在已连接对象的状态读取循环中使用。
sn = robot.getRobotSerialNumber()
model = robot.getRobotModel()
if sn is not None and model is not None:
    print(sn, bpx_sdk.RobotModel(model).name)
```

机型枚举为 `RobotModel.Unknown = 0`、`BPX = 1`、`BPXPro = 2`、`BPW = 3`。
判断是否收到机型时请使用 `model is not None`，因为 `Unknown` 的值是 `0`。

这三个类也提供 `getControlMode()`，返回控制模式整数；未收到反馈或断连时返回 `None`。

```python
mode = robot.getControlMode()
if mode is not None:
    print(bpx_sdk.ControlMode(mode).name)
```

`ControlMode` 取值为 `Unknown=0`、`RemoteControl=1`、`Navigator=2`。

`getChargerIn1()`、`getChargerIn2()` 返回 `0`（未插入）或 `1`（已插入），尚无反馈时返回 `None`。
C++ 和 Python 示例均在状态输出中显示这两个值。

`getHardEstop()` 在机器人硬急停开关按下（所有关节掉电）期间返回 `1`，释放后返回 `0`，
1 Hz 反馈到达前返回 `None`。该状态由 1 Hz 状态标志位 bit 2 携带，需要机器人固件支持
发布。示例的状态输出行中同样包含该值。

`getFaultState()` 返回专用 1 Hz UDP 通道（head `0x00F0`）的全量活动故障快照：
Python 为 `{'entry_count': int, 'truncated': bool, 'entries': [{'code': int,
'occurred_t_ms': int}]}`，C++ 为 `RobotFaultState` 结构体。最多 32 条，按
severity 降序排列，`entries[0]` 即当前最高优先级故障；机器人无故障时仍以
`entry_count=0` 心跳包持续上报。返回 `None`/false 仅表示尚未收到故障包
（例如旧固件）。`occurred_t_ms` 为 Unix epoch 毫秒；可使用 `fault_code.h`
中的位域解码辅助（`DecodeFaultSeverity/Module/SubModule/SubCode`）渲染故障码。
示例输出 `faults=<count>[+truncated] top=0x…` 摘要。需要机器人固件支持该通道；
`getFaultTimestamp()` 返回快照接收时间（单调时钟）。

#### 当前固件实际会报告的故障码

下表列出当前机器人固件在故障通道上**实际会检测并报告**的全部故障码（已逐一对
照检测代码路径核实）。枚举中定义但未列出的码仅作预留，当前固件不会在线上出现。
除特别注明外，`sub_module` 为关节编号（0-11）；severity 编码在故障码中
（当前固件只会出现 `1=Warning`、`2=Error`）。

| 模块 | 子码 | 含义 | sub_module | 典型 severity |
| --- | --- | --- | --- | --- |
| `0x02` 关节 | `0x0001` | 关节位置超限 | 关节编号 | Error |
| `0x02` 关节 | `0x0002` | 关节速度超限 | 关节编号 | Error |
| `0x02` 关节 | `0x0004` | 无关节数据反馈 | 关节编号 | Error |
| `0x02` 关节 | `0x0005` | 电机过温 | 最热关节 | Warning（预警带）/ Error（保护带） |
| `0x02` 关节 | `0x0007` | 非法关节位置反馈（NaN） | 关节编号 | Error |
| `0x02` 关节 | `0x0008` | 非法关节速度反馈（NaN） | 关节编号 | Error |
| `0x02` 关节 | `0x000A` | 关节通信断开 | 关节编号 | Error |
| `0x02` 关节 | `0x000B` | 关节执行不到位 | 关节编号 | Error（站立过程判定） |
| `0x02` 关节 | `0x000C` | 膝关节型号不一致 | `0xFF` | Error |
| `0x02` 关节 | `0x000D` | 关节装配位置错误 | 关节编号 | Error |
| `0x02` 关节 | `0x000E` | 电机温度相对异常（同类关节温差过大） | 关节编号 | Warning |
| `0x02` 关节 | `0x000F` | CAN ID 重复 | 关节编号 | Error |
| `0x02` 关节 | `0x0010` | 关节 SN 不合规 | 关节编号 | Error |
| `0x02` 关节 | `0x0011` | 关节通信同时断连 | `0xFF` | Error |
| `0x03` 驱动器 | `0x0001` | 驱动器过压 | 关节编号 | Warning / Error（需软急停类） |
| `0x03` 驱动器 | `0x0002` | 驱动器欠压 | 关节编号 | Warning / Error（需软急停类） |
| `0x03` 驱动器 | `0x0003` | 驱动器过温 | 关节编号 | Warning（预警带）/ Error（保护带） |
| `0x03` 驱动器 | `0x0004` | 电机堵转保护 | 关节编号 | Warning / Error（需软急停类） |
| `0x03` 驱动器 | `0x0005` | 驱动器温度相对异常（同类关节温差过大） | 关节编号 | Error |
| `0x03` 驱动器 | `0x0006` | 编码器未校准 | 关节编号 | Warning / Error（需软急停类） |
| `0x03` 驱动器 | `0x0007` | ADC 自检失败 | 关节编号 | Warning / Error（需软急停类） |
| `0x04` IMU | `0x0001` | 加速度超限 | `0xFF` | Warning |
| `0x04` IMU | `0x0004` | 无加速度数据 | `0xFF` | Error |
| `0x05` 电池 | `0x0004` | 电池电量过低 | `0xFF` | Error |

当前不会报告（仅枚举预留）：`0x01` 运动控制整组；关节 `0x0003/0x0006/0x0009`；
IMU `0x0002/0x0003`；电池 `0x0001/0x0002/0x0003/0x0005/0x0006`。

`MotionLevelControl` 提供五个跳跃方法，返回值均为 `None`：

| 方法 | 动作 |
| --- | --- |
| `setUpJump()` | 向上跳 |
| `setFrontJump()` | 向前跳 |
| `setBackJump()` | 向后跳 |
| `setLeftJump()` | 向左跳 |
| `setRightJump()` | 向右跳 |

这些动作适用于 BPX。连接就绪后，在机器人进入运动模式且速度指令为零时调用；
每次调用请求一次跳跃。对应步态为 `MotionGait.Jump = 12`。
使用前请安装更新后的 Python 包，并确认机器人固件支持相应功能。

### Python 调用库

Python 封装位于 `bpx_sdk` 包中，直接链接当前项目自带的 C++ 动态库。安装：

```bash
python3 -m pip install .
```

Windows Python 封装只支持 x86_64。构建时会使用 `lib/bpx_sdk_x86_64.lib` 链接原生扩展，并将
`bin/bpx_sdk_x86_64.dll` 安装到 Python 包内。

Linux 下 Python 构建会根据当前机器架构选择 `lib/libbpx_sdk_<arch>.so`。macOS 下 Python 构建会选择
`lib/libbpx_sdk_<arch>.dylib`；当前仓库随包提供的是面向 Apple Silicon 的
`lib/libbpx_sdk_aarch64.dylib`。如需覆盖自动架构检测，可在构建时设置
`BPX_SDK_ARCH=x86_64` 或 `BPX_SDK_ARCH=aarch64`。

使用 `python3 -m pip install .` 从源码安装时，会在本机编译 Python 原生扩展，因此构建机器需要 C++17 编译器和 Python 开发头文件。Windows 源码构建需要安装 Microsoft C++ Build Tools。使用预构建 wheel 安装的用户不需要安装这些编译工具。

#### 构建 Wheel

为当前系统和当前 Python 解释器构建一个 wheel：

```bash
python3 scripts/build_wheels.py --out-dir wheelhouse
```

使用 `cibuildwheel` 为当前系统构建配置范围内的全部 CPython wheel：

```bash
python3.11 scripts/build_wheels.py --cibuildwheel --out-dir wheelhouse
```

运行 `cibuildwheel` 时，宿主 Python 解释器需要为 3.11 或更新版本。

在 Linux 上，可以用一条命令完成环境准备和 release wheel 构建：

```bash
./scripts/build_linux_wheels.sh
```

该命令会使用 `uv` 创建或复用基于 Python 3.11 且包含 `pip` 的
`.venv-cibw-linux`，安装或升级 `cibuildwheel<4`，并将 wheel 输出到
`wheelhouse`。如需更改输出目录，可将目录路径作为第一个参数传入。

在 Windows 上，可使用对应的 PowerShell 脚本：

```powershell
.\scripts\build_windows_wheels.ps1
```

该脚本会使用 `uv` 创建或复用基于 Python 3.11 的 `.venv-cibw-win`，并将
wheel 输出到 `wheelhouse`。如需更改输出目录，可将目录路径作为第一个位置参数传入：

```powershell
.\scripts\build_windows_wheels.ps1 dist\wheels
```

如需在本次构建中忽略代理环境变量和 Windows 系统代理，可使用
`-DisableProxy`；脚本退出时会恢复原来的代理配置：

```powershell
.\scripts\build_windows_wheels.ps1 -DisableProxy
```

在本地 macOS 上，`cibuildwheel` 只会使用 python.org 安装包提供的 CPython
Framework。构建脚本会自动跳过本机未安装的 CPython 版本；GitHub Actions 中仍会构建完整配置矩阵。

`.github/workflows/build-wheels.yml` 中的 GitHub Actions 工作流通过四个并行任务构建 Windows AMD64、Linux x86_64、Linux aarch64 和 macOS arm64 的 wheel 产物。Linux 分别使用原生 `ubuntu-22.04` 和 `ubuntu-22.04-arm` runner，每个任务只构建对应架构，无需 QEMU 模拟。可以在 Actions 页面手动触发，也可以推送 `v*` 标签触发。生成的 wheel 会作为工作流产物上传，安装方式如下：

```bash
pip3 install --no-index --find-links wheelhouse bpx-sdk-open
```

如果 pip 版本过旧，无法识别较新的 wheel 平台标签，安装时可能出现下面的报错：

```text
Looking in links: wheelhouse
ERROR: Could not find a version that satisfies the requirement bpx-sdk-open (from versions: none)
ERROR: No matching distribution found for bpx-sdk-open
```

遇到这个报错时，先升级 pip，然后重新执行安装命令：

```bash
pip3 install --upgrade pip
```

安装完成后，可以用下面的命令验证导入：

```bash
python3 -c "import bpx_sdk; print('ok')"
```

安装完成后可在 Python 中使用和 C++ SDK 对应的三类对象：

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

Python 1.0.10 新增 `MotionLevelControl.setChargingSitDown() -> bool` 和
`MotionState.ChargingSitDown = 24`。更新原生 SDK 头文件、动态库不会自动重建已安装的
Python 扩展，需要执行 `python3 -m pip install --upgrade --force-reinstall .` 重新安装，
或安装 1.0.10 wheel。状态读取仍返回整数或 `None`，可直接与枚举比较。

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

Python 方法名与 C++ SDK 保持一致。读取类接口成功时返回
`list`、`dict`、`int` 或 `float`，暂未收到有效数据时返回 `None`。`getLegOdom()` 返回包含 `velocity_body`、`position`、`orientation`、`angular_velocity` 四个列表字段的 dict。关节控制接口接收长度为 12 的 Python 序列，例如：

```python
joint = bpx_sdk.JointLevelControl()
zeros = [0.0] * bpx_sdk.JOINT_COUNT
kp = [100.0] * bpx_sdk.JOINT_COUNT
kd = [2.0] * bpx_sdk.JOINT_COUNT
joint.setJointCommand(kp, zeros, kd, zeros, zeros)
```

示例文件：
`example/request_robot_state_example.py`、
`example/motion_level_control_example.py`、
`example/joint_level_control_example.py`

## 运行说明

SDK 通过网络与 BPX 实现通讯。请根据「网络连接与 IP」选用正确的机器人 IP；若与 `DEFAULT_SERVER_IP` 不同，请在连接前通过 `setRobotIp` 进行设置。运行程序前，建议先在开发主机上 `ping` 该 IP，确认网络连通后再调用 `connect()`。

### Server/client 示例

`motion_level_control_server_example` 持有 SDK 连接，client 通过文本命令控制服务端。C++ 和 Python 客户端、服务端可以交叉使用。先启动一个服务端，再在另一终端运行客户端：

```bash
# C++（在仓库根目录运行）
./build/motion_level_control_server_example --robot-ip 192.168.1.143
./build/motion_level_control_client_example status

# Python（先安装 Python SDK；与上述 C++ 服务端二选一）
python3 example/motion_level_control_server_example.py --robot-ip 192.168.1.143
python3 example/motion_level_control_client_example.py status
```

服务端默认监听 `127.0.0.1:50051`。跨机器使用时，服务端通过 `--listen-ip` 指定监听地址，客户端通过 `--server-ip` 指定服务端地址；端口分别使用 `--listen-port` 和 `--server-port`。该示例没有认证，仅适用于可信网络。一个服务端依次处理客户端连接；客户端不带命令时进入交互模式。

`status` 返回连接状态、SN、机型、ControlMode、当前/上一运动状态、步态、最大速度以及电池和充电状态；暂不可用的字段显示 `<unavailable>`。服务端每秒打印带本机毫秒时间戳的状态，`battery_level`、`battery_current`、`charger_in1`、`charger_in2` 和 `hard_estop` 位于同一行。

支持 `zero`、`stand`、`sit`、`charging-sit`、`damping`、`stop`、`velocity X Y YAW`、`velocity-control on|off` 和 `gait NAME`；`jump up|front|back|left|right` 分别对应向上、向前、向后、向左、向右跳跃（BPX）。实际可用动作以机型支持为准。客户端 `-h`/`--help` 可在不连接服务端时查看完整命令；连接后输入 `help` 可查看命令及全部步态、跳跃选项，`quit`（或 `shutdown`）关闭服务端。客户端显示的 `OK` 表示命令已被服务端处理，不表示机器人已完成动作。

两种服务端的 `charging-sit` 命令均调用 `setChargingSitDown()`。执行中查询 `status`
可见 `motion_state=24(ChargingSitDown)`；退出到阻尼后可见
`last_motion_state=24(ChargingSitDown)`。

```bash
./build/motion_level_control_client_example charging-sit
./build/motion_level_control_client_example status
python3 example/motion_level_control_client_example.py charging-sit
python3 example/motion_level_control_client_example.py status
```

## 许可证

BPX SDK Open 使用 Apache License 2.0。该许可证覆盖本仓库发布的源码、头文件、
示例、Python 绑定、预编译 BPX SDK 原生库和 wheel。详情见 [LICENSE](LICENSE)、
[NOTICE](NOTICE) 和 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
