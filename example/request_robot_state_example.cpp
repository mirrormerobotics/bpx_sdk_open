#include "request_robot_state.h"
#include "bpx_sdk_version.h"
#include "example_options.h"

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

void printRobotVersion(const bpx_sdk::RequestRobotState& robot_state) {
    uint16_t robot_major = 0;
    uint16_t robot_minor = 0;
    uint16_t robot_patch = 0;
    uint32_t robot_commit = 0;
    uint32_t robot_build = 0;
    uint32_t robot_build_time = 0;
    if (robot_state.getRobotVersion(&robot_major, &robot_minor, &robot_patch,
                                    &robot_commit, &robot_build,
                                    &robot_build_time)) {
        std::cout << "robot version (queried on connect): "
                  << robot_major << "." << robot_minor << "." << robot_patch
                  << " commit=0x" << std::hex << robot_commit
                  << " build=" << std::dec << robot_build
                  << "T" << std::setw(6) << std::setfill('0') << robot_build_time
                  << std::setfill(' ') << std::endl;
    } else {
        std::cout << "robot version: unknown (not supported by robot)" << std::endl;
    }
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

    bpx_sdk::RequestRobotState robot_state;

    // Set the target robot IP.
    robot_state.setRobotIp(options.robot_ip.c_str());

    // Set the local port for receiving robot state packets.
    robot_state.setRobotStateUploadPort(options.robot_state_port);

    // Set the local control connection port. Use 0 for automatic selection.
    robot_state.setTcpLocalPort(options.tcp_local_port);

    // Set the requested robot state upload rate.
    robot_state.setRobotStateUploadRate(options.state_rate_hz);

    std::cout << "robot_ip=" << options.robot_ip
              << " state_port=" << options.robot_state_port
              << " tcp_local_port=" << options.tcp_local_port
              << " state_rate=" << options.state_rate_hz
              << std::endl;

    if (!robot_state.connect()) {
        std::cerr << "failed to connect request robot state" << std::endl;
        return 1;
    }

    printRobotVersion(robot_state);

    while (true) {
        std::cout << "robot state:" << std::endl;

        float joint_pos[12] = {};
        if (robot_state.getJointPosition(joint_pos)) {
            std::cout << "  joint_pos[0]=" << joint_pos[0] << std::endl;
        }

        float rpy[3] = {};
        if (robot_state.getImuRpy(rpy)) {
            std::cout << "  rpy=(" << rpy[0] << ", "
                      << rpy[1] << ", " << rpy[2] << ")"
                      << std::endl;
        }

        bpx_sdk::LegOdom leg_odom {};
        if (robot_state.getLegOdom(&leg_odom)) {
            std::cout << "  leg_odom.position=(" << leg_odom.position[0] << ", "
                      << leg_odom.position[1] << ", " << leg_odom.position[2] << ")"
                      << " velocity_body=(" << leg_odom.velocity_body[0] << ", "
                      << leg_odom.velocity_body[1] << ", " << leg_odom.velocity_body[2] << ")"
                      << " orientation=(" << leg_odom.orientation[0] << ", "
                      << leg_odom.orientation[1] << ", " << leg_odom.orientation[2]
                      << ", " << leg_odom.orientation[3] << ")"
                      << " angular_velocity=(" << leg_odom.angular_velocity[0] << ", "
                      << leg_odom.angular_velocity[1] << ", " << leg_odom.angular_velocity[2] << ")"
                      << std::endl;
        }

        uint8_t current_state = 0;
        uint8_t current_gait = 0;
        uint8_t sub_gait = 0;
        if (robot_state.getCurrentMotionState(&current_state) &&
            robot_state.getCurrentGait(&current_gait) &&
            robot_state.getSubGait(&sub_gait)) {
            std::cout << "  motion_state=" << static_cast<uint32_t>(current_state)
                      << " gait=" << static_cast<uint32_t>(current_gait)
                      << " sub_gait=" << static_cast<int32_t>(static_cast<int8_t>(sub_gait))
                      << std::endl;
        }

        float max_vel[3] = {};
        if (robot_state.getMaxVelocity(max_vel)) {
            std::cout << "  max_vel=(" << max_vel[0] << ", "
                      << max_vel[1] << ", " << max_vel[2] << ")"
                      << std::endl;
        }

        float motor_temperature[12] = {};
        float driver_temperature[12] = {};
        if (robot_state.getMotorTemperature(motor_temperature) &&
            robot_state.getDriverTemperature(driver_temperature)) {
            std::cout << "  motor_temperature[0]=" << motor_temperature[0]
                      << " driver_temperature[0]=" << driver_temperature[0]
                      << std::endl;
        }

        uint8_t battery_level = 0;
        if (robot_state.getBatteryLevel(&battery_level)) {
            std::cout << "  battery_level=" << static_cast<uint32_t>(battery_level) << std::endl;
        }

        float battery_current = 0.0f;
        if (robot_state.getBatteryCurrent(&battery_current)) {
            std::cout << "  battery_current=" << battery_current << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return 0;
}
