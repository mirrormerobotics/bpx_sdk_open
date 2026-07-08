#ifndef BPX_SDK_MOTION_TYPES_H_
#define BPX_SDK_MOTION_TYPES_H_

#include <array>
#include <cstdint>

namespace bpx_sdk {

constexpr int kJointCount = 12;

enum class MotionState : uint8_t {
    LyingDown = 0,
    StandingUp = 1,
    Passive = 2,
    SitDown = 3,
    Motion = 6,
};

enum class MotionGait : uint8_t {
    Walk = 0,
    Bipedal = 3,
    Flip = 4,
    WalkPhase = 6,
    PoseTracking = 7,
    Running = 8,
};

enum class JointIndex : uint8_t {
    FLHipRoll = 0,
    FLHipPitch = 1,
    FLKnee = 2,
    FRHipRoll = 3,
    FRHipPitch = 4,
    FRKnee = 5,
    HLHipRoll = 6,
    HLHipPitch = 7,
    HLKnee = 8,
    HRHipRoll = 9,
    HRHipPitch = 10,
    HRKnee = 11,
};

inline constexpr std::array<const char*, kJointCount> kJointNames = {
    "fl_hip_roll_joint",
    "fl_hip_pitch_joint",
    "fl_knee_joint",
    "fr_hip_roll_joint",
    "fr_hip_pitch_joint",
    "fr_knee_joint",
    "hl_hip_roll_joint",
    "hl_hip_pitch_joint",
    "hl_knee_joint",
    "hr_hip_roll_joint",
    "hr_hip_pitch_joint",
    "hr_knee_joint",
};

constexpr int jointIndex(JointIndex joint) {
    return static_cast<int>(joint);
}

struct LegOdom {
    float velocity_body[3] = {0.0f, 0.0f, 0.0f};       // Body-frame linear velocity, m/s.
    float position[3] = {0.0f, 0.0f, 0.0f};            // World-frame position, m.
    float orientation[4] = {0.0f, 0.0f, 0.0f, 1.0f};   // World-frame orientation, x/y/z/w.
    float angular_velocity[3] = {0.0f, 0.0f, 0.0f};    // Body-frame angular velocity, rad/s.
};

}  // namespace bpx_sdk

#endif  // BPX_SDK_MOTION_TYPES_H_
