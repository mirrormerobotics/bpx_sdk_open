#ifndef BPX_SDK_JOINT_LEVEL_CONTROL_H_
#define BPX_SDK_JOINT_LEVEL_CONTROL_H_

#include "request_robot_state.h"

#include <array>
#include <memory>

namespace bpx_sdk {

class JointLevelControl : public RequestRobotState {
public:
    JointLevelControl();
    ~JointLevelControl() override;

    bool connect() override;
    void disconnect() override;
    void setJointStateUploadPort(uint16_t port) override;

    bool setJointCommand(const std::array<float, 12>& kp,
                         const std::array<float, 12>& pos,
                         const std::array<float, 12>& kd,
                         const std::array<float, 12>& vel,
                         const std::array<float, 12>& tff);
    bool setJointKp(const std::array<float, 12>& kp);
    bool setJointPosition(const std::array<float, 12>& pos);
    bool setJointKd(const std::array<float, 12>& kd);
    bool setJointVelocity(const std::array<float, 12>& vel);
    bool setJointTorqueFeedForward(const std::array<float, 12>& tff);
    bool setZeroJointCommand();

    bool getJointPositionHighRate(float joint_pos[12]) const;
    bool getJointVelocityHighRate(float joint_vel[12]) const;
    bool getJointTorqueHighRate(float joint_tau[12]) const;
    bool getImuRpyHighRate(float rpy[3]) const;
    bool getImuQuatHighRate(float quat[4]) const;
    bool getImuAccHighRate(float acc[3]) const;
    bool getImuOmegaHighRate(float omega[3]) const;
    bool getJointStateTimestampHighRate(float* time_stamp) const;
    bool getJointStateSeqHighRate(uint32_t* seq) const;

private:
    uint8_t hostServerMode() const override;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace bpx_sdk

#endif  // BPX_SDK_JOINT_LEVEL_CONTROL_H_
