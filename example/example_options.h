#ifndef BPX_SDK_EXAMPLE_OPTIONS_H_
#define BPX_SDK_EXAMPLE_OPTIONS_H_

#include "bpx_sdk_config.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace bpx_sdk {
namespace example {

struct Options {
    std::string robot_ip = DEFAULT_SERVER_IP;
    uint16_t robot_state_port = DEFAULT_CLIENT_ROBOT_STATE_UDP_PORT;
    uint16_t joint_state_port = DEFAULT_CLIENT_JOINT_STATE_UDP_PORT;
    uint16_t tcp_local_port = 0;
    uint16_t state_rate_hz = 100;
    bool help_requested = false;
};

inline void printUsage(const char* program, bool enable_joint_state_port) {
    std::cout
        << "Usage: " << program << " [options]\n"
        << "\n"
        << "Options:\n"
        << "  --robot-ip IP              Robot IP address (default: " << DEFAULT_SERVER_IP << ")\n"
        << "  --state-port PORT          Local UDP port for robot state (default: "
        << DEFAULT_CLIENT_ROBOT_STATE_UDP_PORT << ")\n"
        << "  --tcp-local-port PORT      Local TCP port, 0 for automatic selection (default: 0)\n"
        << "  --state-rate HZ            Requested robot state upload rate (default: 100)\n";
    if (enable_joint_state_port) {
        std::cout
            << "  --joint-state-port PORT    Local UDP port for high-rate joint state (default: "
            << DEFAULT_CLIENT_JOINT_STATE_UDP_PORT << ")\n";
    }
    std::cout
        << "  -h, --help                 Show this help\n";
}

inline bool parseUint16(const char* text, uint16_t* value) {
    if (!text || !value || text[0] == '\0') {
        return false;
    }

    char* end = nullptr;
    const unsigned long parsed = std::strtoul(text, &end, 10);
    if (*end != '\0' || parsed > std::numeric_limits<uint16_t>::max()) {
        return false;
    }

    *value = static_cast<uint16_t>(parsed);
    return true;
}

inline bool requireValue(int argc, char** argv, int index) {
    if (index + 1 < argc) {
        return true;
    }
    std::cerr << argv[index] << " requires a value" << std::endl;
    return false;
}

inline bool parseOptions(int argc, char** argv, bool enable_joint_state_port, Options* options) {
    if (!options) {
        return false;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0], enable_joint_state_port);
            options->help_requested = true;
            return true;
        }
        if (arg == "--robot-ip") {
            if (!requireValue(argc, argv, i)) return false;
            options->robot_ip = argv[++i];
            continue;
        }
        if (arg == "--state-port" || arg == "--robot-state-port") {
            if (!requireValue(argc, argv, i) ||
                !parseUint16(argv[++i], &options->robot_state_port)) {
                std::cerr << arg << " expects a port in range 0..65535" << std::endl;
                return false;
            }
            continue;
        }
        if (arg == "--tcp-local-port") {
            if (!requireValue(argc, argv, i) ||
                !parseUint16(argv[++i], &options->tcp_local_port)) {
                std::cerr << arg << " expects a port in range 0..65535" << std::endl;
                return false;
            }
            continue;
        }
        if (arg == "--state-rate") {
            if (!requireValue(argc, argv, i) ||
                !parseUint16(argv[++i], &options->state_rate_hz) ||
                options->state_rate_hz == 0) {
                std::cerr << arg << " expects a positive rate in range 1..65535" << std::endl;
                return false;
            }
            continue;
        }
        if (arg == "--joint-state-port" && enable_joint_state_port) {
            if (!requireValue(argc, argv, i) ||
                !parseUint16(argv[++i], &options->joint_state_port)) {
                std::cerr << arg << " expects a port in range 0..65535" << std::endl;
                return false;
            }
            continue;
        }

        std::cerr << "Unknown option: " << arg << std::endl;
        printUsage(argv[0], enable_joint_state_port);
        return false;
    }

    return true;
}

}  // namespace example
}  // namespace bpx_sdk

#endif  // BPX_SDK_EXAMPLE_OPTIONS_H_
