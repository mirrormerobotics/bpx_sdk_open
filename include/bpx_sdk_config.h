#ifndef BPX_SDK_BPX_SDK_CONFIG_H_
#define BPX_SDK_BPX_SDK_CONFIG_H_

#include <cstdint>

namespace bpx_sdk {

// Client-side UDP ports listened to by this SDK for robot state upload data.
constexpr uint16_t DEFAULT_CLIENT_ROBOT_STATE_UDP_PORT = 9873;
constexpr uint16_t DEFAULT_CLIENT_JOINT_STATE_UDP_PORT = 7895;

// Default robot server IP used by this SDK.
constexpr char DEFAULT_SERVER_IP[] = "10.21.20.1";

}  // namespace bpx_sdk

#endif  // BPX_SDK_BPX_SDK_CONFIG_H_
