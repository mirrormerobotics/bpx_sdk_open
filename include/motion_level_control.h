#ifndef BPX_SDK_MOTION_LEVEL_CONTROL_H_
#define BPX_SDK_MOTION_LEVEL_CONTROL_H_

#include "request_robot_state.h"

#include <memory>

namespace bpx_sdk {

class MotionLevelControl : public RequestRobotState {
public:
    MotionLevelControl();
    ~MotionLevelControl() override;

    bool connect() override;
    void disconnect() override;

    void setMotionCommandRate(uint16_t rate_hz);
    void setVelocityControlFlag(bool enabled);
    void setZeroPositionsFlag();
    void setWalk();
    void setRunning();
    void setLeftFlip();
    void setRightFlip();
    void setBipedal();
    void setInvBipedal();
    void setPronk();
    void setPace();
    void setBound();
    bool setVelocity(float x, float y, float yaw);
    bool setStandUp();
    bool setSitDown();
    bool setDamping();
    bool setUpright();

private:
    uint8_t hostServerMode() const override;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace bpx_sdk

#endif  // BPX_SDK_MOTION_LEVEL_CONTROL_H_
