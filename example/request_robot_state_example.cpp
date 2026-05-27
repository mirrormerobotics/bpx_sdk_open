#include "request_robot_state.h"
#include "bpx_sdk_version.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

namespace {

void printVersion() {
    std::cout
        << BPX_SDK_PROJECT_NAME
        << " version=" << BPX_SDK_PROJECT_VERSION
        << std::endl;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 1) {
        std::cerr << argv[0] << " does not accept command line arguments." << std::endl;
        return 1;
    }

    printVersion();

    bpx_sdk::RequestRobotState robot_state;

    // Set the target robot IP.
    robot_state.setRobotIp(bpx_sdk::DEFAULT_SERVER_IP);

    // Set the local port for receiving robot state packets.
    robot_state.setRobotStateUploadPort(bpx_sdk::DEFAULT_CLIENT_ROBOT_STATE_UDP_PORT);

    // Set the local control connection port. Use 0 for automatic selection.
    robot_state.setTcpLocalPort(0);

    // Set the requested robot state upload rate.
    robot_state.setRobotStateUploadRate(100);

    if (!robot_state.connect()) {
        std::cerr << "failed to connect request robot state" << std::endl;
        return 1;
    }

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

        float vel_body[3] = {};
        if (robot_state.getCurrentVelocityBody(vel_body)) {
            std::cout << "  vel_body=(" << vel_body[0] << ", "
                      << vel_body[1] << ", " << vel_body[2] << ")"
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
