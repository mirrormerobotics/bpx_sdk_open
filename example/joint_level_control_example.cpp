#include "joint_level_control.h"
#include "bpx_sdk_version.h"
#include "example_options.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>

namespace {

void printVersion() {
    std::cout
        << BPX_SDK_PROJECT_NAME
        << " version=" << BPX_SDK_PROJECT_VERSION
        << std::endl;
}

void printRobotStatus(bpx_sdk::JointLevelControl& joint_level_control) {
    uint8_t current_state = 0;
    uint8_t current_gait = 0;
    uint8_t battery_level = 0;
    float joint_timestamp = 0.0f;
    uint32_t joint_seq = 0;
    std::array<float, 3> rpy{};
    std::array<float, 12> joint_pos{};
    std::array<float, 12> joint_vel{};
    std::array<float, 12> joint_tau{};

    const bool has_motion_state =
        joint_level_control.getCurrentMotionState(&current_state) &&
        joint_level_control.getCurrentGait(&current_gait);
    const bool has_battery = joint_level_control.getBatteryLevel(&battery_level);
    const bool has_joint_time =
        joint_level_control.getJointStateTimestampHighRate(&joint_timestamp) &&
        joint_level_control.getJointStateSeqHighRate(&joint_seq);
    const bool has_rpy = joint_level_control.getImuRpyHighRate(rpy.data());
    const bool has_joint =
        joint_level_control.getJointPositionHighRate(joint_pos.data()) &&
        joint_level_control.getJointVelocityHighRate(joint_vel.data()) &&
        joint_level_control.getJointTorqueHighRate(joint_tau.data());

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "[state]";
    if (has_motion_state) {
        std::cout << " motion=" << static_cast<int>(current_state)
                  << " gait=" << static_cast<int>(current_gait);
    }
    if (has_battery) {
        std::cout << " battery=" << static_cast<int>(battery_level) << "%";
    }
    if (has_joint_time) {
        std::cout << " high_rate_seq=" << joint_seq
                  << " high_rate_t=" << joint_timestamp;
    }
    if (has_rpy) {
        std::cout << " rpy=(" << rpy[0] << ", " << rpy[1] << ", " << rpy[2] << ")";
    }
    if (has_joint) {
        std::cout << " q[0..5]=(";
        for (int i = 0; i < 6; ++i) {
            std::cout << (i == 0 ? "" : ", ") << joint_pos[i];
        }
        std::cout << ") dq[0..5]=(";
        for (int i = 0; i < 6; ++i) {
            std::cout << (i == 0 ? "" : ", ") << joint_vel[i];
        }
        std::cout << ") tau[0..5]=(";
        for (int i = 0; i < 6; ++i) {
            std::cout << (i == 0 ? "" : ", ") << joint_tau[i];
        }
        std::cout << ")";
    }
    std::cout << std::endl;
}

std::array<float, 12> makeLegTarget(float abad, float hip, float knee) {
    std::array<float, 12> target{};
    for (int leg = 0; leg < 4; ++leg) {
        target[3 * leg + 0] = abad;
        target[3 * leg + 1] = hip;
        target[3 * leg + 2] = knee;
    }
    return target;
}

bool sendPositionCommand(bpx_sdk::JointLevelControl& joint_level_control,
                         const std::array<float, 12>& pos) {
    return joint_level_control.setJointPosition(pos);
}

bool runHoldStage(bpx_sdk::JointLevelControl& joint_level_control,
                  const std::array<float, 12>& hold_pos,
                  std::chrono::milliseconds duration,
                  std::chrono::steady_clock::time_point& next_status_print) {
    const auto status_print_period = std::chrono::milliseconds(200);
    const auto start = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() - start < duration) {
        if (!sendPositionCommand(joint_level_control, hold_pos)) {
            std::cerr << "failed to send hold joint command" << std::endl;
            return false;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= next_status_print) {
            printRobotStatus(joint_level_control);
            next_status_print = now + status_print_period;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return true;
}

bool runMoveStage(bpx_sdk::JointLevelControl& joint_level_control,
                  const std::array<float, 12>& start_pos,
                  const std::array<float, 12>& target_pos,
                  std::chrono::milliseconds duration,
                  std::chrono::steady_clock::time_point& next_status_print) {
    const auto status_print_period = std::chrono::milliseconds(200);
    const auto start = std::chrono::steady_clock::now();
    std::array<float, 12> cmd_pos{};

    while (true) {
        const auto elapsed = std::chrono::steady_clock::now() - start;
        float alpha = static_cast<float>(
            std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()) /
            static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(duration).count());
        if (alpha > 1.0f) {
            alpha = 1.0f;
        }

        for (int i = 0; i < 12; ++i) {
            cmd_pos[i] = start_pos[i] + alpha * (target_pos[i] - start_pos[i]);
        }

        if (!sendPositionCommand(joint_level_control, cmd_pos)) {
            std::cerr << "failed to send moving joint command" << std::endl;
            return false;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= next_status_print) {
            printRobotStatus(joint_level_control);
            next_status_print = now + status_print_period;
        }

        if (alpha >= 1.0f) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return true;
}

}  // namespace

int main(int argc, char** argv) {
    bpx_sdk::example::Options options;
    if (!bpx_sdk::example::parseOptions(argc, argv, true, &options)) {
        return 1;
    }
    if (options.help_requested) {
        return 0;
    }

    printVersion();

    bpx_sdk::JointLevelControl joint_level_control;

    // Set the robot address.
    joint_level_control.setRobotIp(options.robot_ip.c_str());

    // Set the local state receiving port.
    joint_level_control.setRobotStateUploadPort(options.robot_state_port);
    joint_level_control.setJointStateUploadPort(options.joint_state_port);

    // Use 0 to let the system choose the local connection port automatically.
    joint_level_control.setTcpLocalPort(options.tcp_local_port);

    // Set the requested robot state upload rate.
    joint_level_control.setRobotStateUploadRate(options.state_rate_hz);

    std::cout << "robot_ip=" << options.robot_ip
              << " state_port=" << options.robot_state_port
              << " joint_state_port=" << options.joint_state_port
              << " tcp_local_port=" << options.tcp_local_port
              << " state_rate=" << options.state_rate_hz
              << std::endl;

    if (!joint_level_control.connect()) {
        std::cerr << "failed to connect joint level control" << std::endl;
        return 1;
    }

    std::array<float, 12> init_pos{};
    while (!joint_level_control.getJointPositionHighRate(init_pos.data())) {
        std::cout << "waiting for initial high-rate joint position..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    const auto first_target_pos = makeLegTarget(0.0f, 0.8f, -1.8f);
    const auto second_target_pos = makeLegTarget(0.0f, 1.2f, -2.7f);

    std::array<float, 12> kp{};
    std::array<float, 12> kd{};
    kp.fill(100.0f);
    kd.fill(2.0f);
    std::array<float, 12> vel{};
    std::array<float, 12> tff{};
    if (!joint_level_control.setJointCommand(kp, init_pos, kd, vel, tff)) {
        std::cerr << "failed to set initial joint gains" << std::endl;
        joint_level_control.disconnect();
        return 1;
    }

    auto next_status_print = std::chrono::steady_clock::now();
    const auto hold_duration = std::chrono::seconds(1);
    const auto move_duration = std::chrono::seconds(2);

    std::cout << "hold current joint position for 1s" << std::endl;
    if (!runHoldStage(joint_level_control, init_pos, hold_duration, next_status_print)) {
        joint_level_control.disconnect();
        return 1;
    }

    std::cout << "move to first target position in 2s" << std::endl;
    if (!runMoveStage(joint_level_control, init_pos, first_target_pos, move_duration, next_status_print)) {
        joint_level_control.disconnect();
        return 1;
    }

    std::cout << "hold first target position for 1s" << std::endl;
    if (!runHoldStage(joint_level_control, first_target_pos, hold_duration, next_status_print)) {
        joint_level_control.disconnect();
        return 1;
    }

    std::cout << "move every leg to (0, 1.2, -2.7) in 2s" << std::endl;
    if (!runMoveStage(joint_level_control, first_target_pos, second_target_pos, move_duration, next_status_print)) {
        joint_level_control.disconnect();
        return 1;
    }

    std::cout << "hold final target position for 1s" << std::endl;
    if (!runHoldStage(joint_level_control, second_target_pos, hold_duration, next_status_print)) {
        joint_level_control.disconnect();
        return 1;
    }

    joint_level_control.setZeroJointCommand();
    joint_level_control.disconnect();
    return 0;
}
