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
    // Request pose tracking with all six axes zero. True means sent, not entered;
    // wait for MotionGait::PoseTracking feedback before sending nonzero axes.
    bool setPoseTracking();
    // Four normalized inputs in x, y, z, r order; r is the yaw input.
    // Sends [x, y, r, z, 0, 0] to the controller; unused channels stay zero.
    // Each value must be finite and in [-1, 1]; these are not SI pose values.
    // The robot applies its pose-axis mapping (including X/Y swap and X inversion).
    // Requires setPoseTracking() in the current connection; returns false on
    // invalid input, an unavailable session, another selected gait, or send failure.
    // Uses normalized control independently of setVelocityControlFlag().
    bool setPose(float x, float y, float z, float r);
    void setLeftFlip();
    void setRightFlip();
    // BPX only; BPX-Pro ignores bipedal commands.
    void setBipedal();
    void setInvBipedal();
    // BPX jump actions; each call requests one jump.
    void setUpJump();
    void setFrontJump();
    void setBackJump();
    void setLeftJump();
    void setRightJump();
    void setPronk();
    void setPace();
    void setBound();
    bool setVelocity(float x, float y, float yaw);
    bool setStandUp();
    bool setSitDown();
    // Request charging sit-down. A successful send does not mean completion;
    // observe MotionState::ChargingSitDown through the motion-state getters.
    bool setChargingSitDown();
    bool setDamping();

private:
    uint8_t hostServerMode() const override;
    void onStateConnectionChanged(bool connected) override;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace bpx_sdk

#endif  // BPX_SDK_MOTION_LEVEL_CONTROL_H_
