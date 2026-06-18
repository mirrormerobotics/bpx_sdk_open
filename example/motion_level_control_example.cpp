#include "motion_level_control.h"
#include "bpx_sdk_version.h"
#include "example_options.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

namespace {

enum class DemoPhase {
    kWait,
    kStandUp,
    kYawVelocity,
    kZeroVelocity,
    kGaitInput,
    kSitDown,
};

void printVersion() {
    std::cout
        << BPX_SDK_PROJECT_NAME
        << " version=" << BPX_SDK_PROJECT_VERSION
        << std::endl;
}

}  // namespace

int main(int argc, char** argv) {
    bpx_sdk::example::Options options;
    if (!bpx_sdk::example::parseOptions(argc, argv, false, &options)) {
        return 1;
    }
    if (options.help_requested) {
        return 0;
    }

    printVersion();

    bpx_sdk::MotionLevelControl motion_level_control;

    // Set the target robot IP.
    motion_level_control.setRobotIp(options.robot_ip.c_str());

    // Set the local port for receiving robot state packets.
    motion_level_control.setRobotStateUploadPort(options.robot_state_port);

    // Set the local control connection port. Use 0 for automatic selection.
    motion_level_control.setTcpLocalPort(options.tcp_local_port);

    // Set the requested robot state upload rate.
    motion_level_control.setRobotStateUploadRate(options.state_rate_hz);

    // Set the motion command send rate and velocity control mode.
    motion_level_control.setMotionCommandRate(50);

    std::cout << "robot_ip=" << options.robot_ip
              << " state_port=" << options.robot_state_port
              << " tcp_local_port=" << options.tcp_local_port
              << " state_rate=" << options.state_rate_hz
              << std::endl;

    if (!motion_level_control.connect()) {
        std::cerr << "failed to connect motion level control" << std::endl;
        return 1;
    }

    std::cout << "motion level control running" << std::endl;
    DemoPhase phase = DemoPhase::kWait;
    auto phase_start = std::chrono::steady_clock::now();
    bool phase_announced = false;
    while (true) {
        const auto now = std::chrono::steady_clock::now();
        const auto phase_elapsed = now - phase_start;
        uint8_t current_state = 0;
        uint8_t current_gait = 0;
        float max_vel[3] = {};
        const bool has_motion_state =
            motion_level_control.getCurrentMotionState(&current_state) &&
            motion_level_control.getCurrentGait(&current_gait) &&
            motion_level_control.getMaxVelocity(max_vel);
        if (has_motion_state) {
            std::cout << "motion_state=" << static_cast<uint32_t>(current_state)
                      << " gait=" << static_cast<uint32_t>(current_gait)
                      << " max_vel=(" << max_vel[0] << ", "
                      << max_vel[1] << ", " << max_vel[2] << ")"
                      << std::endl;
        }

        switch (phase) {
            case DemoPhase::kWait:
                if (!phase_announced) {
                    motion_level_control.setZeroPositionsFlag();
                    std::cout << "send zero-position command" << std::endl;
                    std::cout << "wait 1 second before sending commands" << std::endl;
                    phase_announced = true;
                }
                if (phase_elapsed >= std::chrono::seconds(1)) {
                    phase = DemoPhase::kStandUp;
                    phase_start = now;
                    phase_announced = false;
                }
                break;

            case DemoPhase::kStandUp:
                if (!phase_announced) {
                    std::cout << "send stand-up command" << std::endl;
                    phase_announced = true;
                }
                if (!motion_level_control.setStandUp()) {
                    std::cerr << "failed to send stand-up command" << std::endl;
                    return 1;
                }
                if (has_motion_state && current_state == 6) {
                    phase = DemoPhase::kYawVelocity;
                    phase_start = now;
                    phase_announced = false;
                    motion_level_control.setVelocityControlFlag(true);
                }
                break;

            case DemoPhase::kYawVelocity:
                if (!phase_announced) {
                    std::cout << "send yaw velocity command for 3 seconds" << std::endl;
                    phase_announced = true;
                }
                if (!motion_level_control.setVelocity(0.0f, 0.0f, 1.0f)) {
                    std::cerr << "failed to send yaw velocity command" << std::endl;
                    return 1;
                }
                if (phase_elapsed >= std::chrono::seconds(3)) {
                    phase = DemoPhase::kZeroVelocity;
                    phase_start = now;
                    phase_announced = false;
                    motion_level_control.setVelocityControlFlag(false);
                }
                break;

            case DemoPhase::kZeroVelocity:
                if (!phase_announced) {
                    std::cout << "send zero velocity command for 2 seconds" << std::endl;
                    phase_announced = true;
                }
                if (!motion_level_control.setVelocity(0.0f, 0.0f, 0.0f)) {
                    std::cerr << "failed to send zero velocity command" << std::endl;
                    return 1;
                }
                if (phase_elapsed >= std::chrono::seconds(2)) {
                    phase = DemoPhase::kGaitInput;
                    // phase = DemoPhase::kSitDown;
                    phase_start = now;
                    phase_announced = false;
                }
                break;
            
            case DemoPhase::kGaitInput:
                if (!phase_announced) {
                    std::cout << "send left-flip command for 2 seconds" << std::endl;
                    phase_announced = true;
                    // motion_level_control.setLeftFlip();
                    // motion_level_control.setRightFlip();
                    // motion_level_control.setBound();
                    // motion_level_control.setInvBipedal();
                }
                if (phase_elapsed >= std::chrono::seconds(1)) {
                    motion_level_control.setWalk();
                    phase = DemoPhase::kSitDown;
                    phase_start = now;
                    phase_announced = false;
                }
                break;

            case DemoPhase::kSitDown:
                if (!phase_announced) {
                    std::cout << "send sit-down command, exit after 1 second" << std::endl;
                    phase_announced = true;
                }
                if (!motion_level_control.setSitDown()) {
                    std::cerr << "failed to send sit-down command" << std::endl;
                    return 1;
                }
                if (phase_elapsed >= std::chrono::seconds(5)) {
                    motion_level_control.disconnect();
                    return 0;
                }
                break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return 0;
}
