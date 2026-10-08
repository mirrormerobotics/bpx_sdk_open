#ifndef BPX_SDK_EXAMPLE_FEEDBACK_H_
#define BPX_SDK_EXAMPLE_FEEDBACK_H_

#include "request_robot_state.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

namespace bpx_sdk::example {

inline void printIdentityAvailability(const RequestRobotState& robot) {
    const auto sn = robot.getRobotSerialNumberValue();
    const auto model = robot.getRobotModelValue();
    // Successful identity queries are already printed by the SDK in connect().
    if (!sn || !model) {
        std::cout << "robot identity: <unavailable>" << std::endl;
    }
}

inline std::string formatControlMode(const RequestRobotState& robot) {
    ControlMode mode;
    if (!robot.getControlMode(&mode)) return "<unavailable>";
    const char* name = "Unknown";
    switch (mode) {
        case ControlMode::RemoteControl: name = "RemoteControl"; break;
        case ControlMode::Navigator: name = "Navigator"; break;
        case ControlMode::Unknown: break;
    }
    return std::string(name) + " (" + std::to_string(static_cast<unsigned>(mode)) + ")";
}

inline std::string formatPowerState(const RequestRobotState& robot) {
    const auto level = robot.getBatteryLevelValue();
    const auto current = robot.getBatteryCurrentValue();
    const auto in1 = robot.getChargerIn1Value();
    const auto in2 = robot.getChargerIn2Value();
    const auto hard_estop = robot.getHardEstopValue();
    const auto fault_state = robot.getFaultStateValue();
    std::ostringstream output;
    output << "battery_level=" << (level ? std::to_string(*level) + "%" : "<unavailable>")
           << " battery_current=";
    if (current) output << std::fixed << std::setprecision(3) << *current;
    else output << "<unavailable>";
    output << " charger_in1=" << (in1 ? std::to_string(*in1) : "<unavailable>")
           << " charger_in2=" << (in2 ? std::to_string(*in2) : "<unavailable>")
           << " hard_estop=" << (hard_estop ? std::to_string(*hard_estop) : "<unavailable>");
    if (fault_state) {
        output << " faults=" << fault_state->entry_count
               << (fault_state->truncated ? "+truncated" : "");
        if (fault_state->entry_count > 0) {
            output << " top=0x" << std::hex << fault_state->entries[0].code << std::dec;
        }
    } else {
        output << " faults=<unavailable>";
    }
    return output.str();
}

}  // namespace bpx_sdk::example
#endif
