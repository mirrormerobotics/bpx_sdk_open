#include "motion_level_control.h"
#include "example_options.h"

#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace std::chrono_literals;

constexpr auto kCommandInterval = 20ms;
constexpr auto kSweepDuration = 3s;
constexpr auto kEndpointHold = 1s;
constexpr auto kNeutralPause = 1s;
constexpr auto kPostureTimeout = 10s;
constexpr int kSweepSteps = kSweepDuration / kCommandInterval;

void SweepPoseAxis(bpx_sdk::MotionLevelControl& motion, size_t axis_index,
                   const char* axis_name) {
    const auto start = std::chrono::steady_clock::now();
    for (int step = 0; step <= kSweepSteps; ++step) {
        // Send -1000 immediately, hold it, then start the timed sweep.
        if (step > 0) {
            std::this_thread::sleep_until(start + kEndpointHold + step * kCommandInterval);
        }
        const int value = -1000 + 2000 * step / kSweepSteps;
        std::array<float, 4> axes{};
        axes[axis_index] = value / 1000.0f;
        if (!motion.setPose(axes[0], axes[1], axes[2], axes[3])) throw std::runtime_error("pose sweep command failed");
        std::array<int, 4> display{};
        display[axis_index] = value;
        std::cout << "axis=" << axis_name
                  << " x=" << display[0] << " y=" << display[1]
                  << " z=" << display[2] << " r=" << display[3] << std::endl;
    }
    // Hold +1000 before returning to neutral and starting the next axis.
    std::this_thread::sleep_for(kEndpointHold);
    if (!motion.setPose(0.0f, 0.0f, 0.0f, 0.0f)) throw std::runtime_error("neutral pose command failed");
    std::cout << "axis=" << axis_name << " complete x=0 y=0 z=0 r=0" << std::endl;
    std::this_thread::sleep_for(kNeutralPause);
}

template <typename Predicate>
void WaitUntil(Predicate ready, const char* error,
               std::chrono::milliseconds timeout = 3s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!ready()) {
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error(error);
        std::this_thread::sleep_for(20ms);
    }
}

int main(int argc, char** argv) {
    bpx_sdk::example::Options options;
    if (!bpx_sdk::example::parseOptions(argc, argv, false, &options)) {
        return 1;
    }
    if (options.help_requested) {
        std::cout << "Automatically stand up, run four pose sweeps, then lie down.\n";
        return 0;
    }
    bpx_sdk::MotionLevelControl motion;
    motion.setRobotIp(options.robot_ip.c_str());
    motion.setRobotStateUploadPort(options.robot_state_port);
    motion.setTcpLocalPort(options.tcp_local_port);
    motion.setRobotStateUploadRate(options.state_rate_hz);
    motion.setMotionCommandRate(50);
    std::cout << "robot_ip=" << options.robot_ip
              << " state_port=" << options.robot_state_port
              << " tcp_local_port=" << options.tcp_local_port
              << " state_rate=" << options.state_rate_hz << '\n';
    try {
        if (!motion.connect()) throw std::runtime_error("connection failed");
        std::cout << "request stand-up" << std::endl;
        WaitUntil([&] { return motion.setStandUp(); }, "stand-up request failed");
        WaitUntil([&] {
            return motion.getCurrentMotionStateEnum() == bpx_sdk::MotionState::Motion;
        }, "robot did not finish standing up", kPostureTimeout);
        std::cout << "stand-up complete; request pose tracking" << std::endl;
        WaitUntil([&] { return motion.setPoseTracking(); }, "pose request failed");
        WaitUntil([&] {
            return motion.getCurrentGaitEnum() == bpx_sdk::MotionGait::PoseTracking;
        }, "robot did not enter pose tracking");
        // Public API order is x, y, z, r.
        SweepPoseAxis(motion, 0, "x");
        SweepPoseAxis(motion, 1, "y");
        SweepPoseAxis(motion, 2, "z");
        SweepPoseAxis(motion, 3, "r");
        motion.setWalk();
        WaitUntil([&] {
            return motion.getCurrentGaitEnum() == bpx_sdk::MotionGait::Walk;
        }, "robot did not return to walking gait");
        std::cout << "pose sweeps complete; request lying down" << std::endl;
        WaitUntil([&] { return motion.setSitDown(); }, "lying-down request failed");
        WaitUntil([&] {
            return motion.getCurrentMotionStateEnum() == bpx_sdk::MotionState::LyingDown;
        }, "robot did not finish lying down", kPostureTimeout);
        std::cout << "lying-down complete" << std::endl;
    } catch (const std::exception& error) {
        motion.setPose(0.0f, 0.0f, 0.0f, 0.0f);
        motion.disconnect();
        std::cerr << error.what() << '\n';
        return 1;
    }
    motion.disconnect();
    return 0;
}
