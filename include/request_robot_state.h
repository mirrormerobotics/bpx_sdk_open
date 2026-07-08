#ifndef BPX_SDK_REQUEST_ROBOT_STATE_H_
#define BPX_SDK_REQUEST_ROBOT_STATE_H_

#include "bpx_sdk_config.h"
#include "motion_types.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace bpx_sdk {

class RequestRobotState {
public:
    RequestRobotState();
    virtual ~RequestRobotState();

    virtual bool connect();
    virtual void disconnect();
    void setRobotStateUploadPort(uint16_t);
    virtual void setJointStateUploadPort(uint16_t);
    void setRobotStateUploadRate(uint16_t rate_hz);
    void setTcpLocalPort(uint16_t port);
    void setSessionId(uint16_t);
    virtual void setRobotIp(const char*);

    // Query the robot (motion host) build version. Uses an independent TCP
    // connection, so it may be called before connect(). Returns false when the
    // robot does not support version query. All output pointers are optional.
    bool queryRobotVersion(uint16_t* major, uint16_t* minor, uint16_t* patch,
                           uint32_t* commit = nullptr, uint32_t* build_date = nullptr,
                           uint32_t* build_time = nullptr);

    // Read the robot version cached during connect() (no network I/O). Returns
    // false if connect() has not yet succeeded or the robot did not support
    // version query. Use queryRobotVersion() for a fresh on-demand query.
    bool getRobotVersion(uint16_t* major, uint16_t* minor, uint16_t* patch,
                         uint32_t* commit = nullptr, uint32_t* build_date = nullptr,
                         uint32_t* build_time = nullptr) const;

    bool getJointPosition(float joint_pos[12]) const;
    bool getJointVelocity(float joint_vel[12]) const;
    bool getJointTorque(float joint_tau[12]) const;

    bool getImuRpy(float rpy[3]) const;    // ZYX Euler angles: roll, pitch, yaw.
    bool getImuQuat(float quat[4]) const;  // Quaternion order: x, y, z, w.
    bool getImuAcc(float acc[3]) const;
    bool getImuOmega(float omega[3]) const;

    bool getLegOdom(LegOdom* leg_odom) const;

    bool getMotorTemperature(float motor_temperature[12]) const;
    bool getDriverTemperature(float driver_temperature[12]) const;
    bool getCurrentMotionState(uint8_t* current_state) const;
    bool getCurrentGait(uint8_t* current_gait) const;
    bool getLastMotionState(uint8_t* last_state) const;
    bool getLastGait(uint8_t* last_gait) const;
    bool getSubGait(uint8_t* sub_gait) const;
    bool getCurrentMotionState(MotionState* current_state) const;
    bool getCurrentGait(MotionGait* current_gait) const;
    bool getLastMotionState(MotionState* last_state) const;
    bool getLastGait(MotionGait* last_gait) const;
    bool getMaxVelocity(float max_vel[3]) const;

    bool getBatteryLevel(uint8_t* battery_level) const;
    bool getBatteryCurrent(float* battery_current) const;
    bool getJointStateTimestamp(uint32_t* time_ms) const;
    bool getImuTimestamp(uint32_t* time_ms) const;
    bool getOdometryTimestamp(uint32_t* time_ms) const;
    bool getMotionStateTimestamp(uint32_t* time_ms) const;
    bool getBatteryTimestamp(uint32_t* time_ms) const;

    std::optional<std::array<float, 12>> getJointPositionArray() const;
    std::optional<std::array<float, 12>> getJointVelocityArray() const;
    std::optional<std::array<float, 12>> getJointTorqueArray() const;

    std::optional<std::array<float, 3>> getImuRpyArray() const;    // ZYX Euler angles: roll, pitch, yaw.
    std::optional<std::array<float, 4>> getImuQuatArray() const;   // Quaternion order: x, y, z, w.
    std::optional<std::array<float, 3>> getImuAccArray() const;
    std::optional<std::array<float, 3>> getImuOmegaArray() const;

    std::optional<LegOdom> getLegOdomValue() const;

    std::optional<std::array<float, 12>> getMotorTemperatureArray() const;
    std::optional<std::array<float, 12>> getDriverTemperatureArray() const;
    std::optional<uint8_t> getCurrentMotionStateValue() const;
    std::optional<uint8_t> getCurrentGaitValue() const;
    std::optional<uint8_t> getLastMotionStateValue() const;
    std::optional<uint8_t> getLastGaitValue() const;
    std::optional<uint8_t> getSubGaitValue() const;
    std::optional<MotionState> getCurrentMotionStateEnum() const;
    std::optional<MotionGait> getCurrentGaitEnum() const;
    std::optional<MotionState> getLastMotionStateEnum() const;
    std::optional<MotionGait> getLastGaitEnum() const;
    std::optional<std::array<float, 3>> getMaxVelocityArray() const;

    std::optional<uint8_t> getBatteryLevelValue() const;
    std::optional<float> getBatteryCurrentValue() const;
    std::optional<uint32_t> getJointStateTimestampValue() const;
    std::optional<uint32_t> getImuTimestampValue() const;
    std::optional<uint32_t> getOdometryTimestampValue() const;
    std::optional<uint32_t> getMotionStateTimestampValue() const;
    std::optional<uint32_t> getBatteryTimestampValue() const;

protected:
    virtual uint8_t hostServerMode() const;
    const char* robotIp() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace bpx_sdk

#endif  // BPX_SDK_REQUEST_ROBOT_STATE_H_
