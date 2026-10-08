#ifndef BPX_SDK_ROBOT_IDENTITY_PROTOCOL_H_
#define BPX_SDK_ROBOT_IDENTITY_PROTOCOL_H_

#include "host_server_modes.h"
#include <cstddef>
#include <cstdint>

namespace bpx_protocol {
// TCP response to a 24-byte SubscribeStateReq with mode=IdentityQuery.
// SN is printable ASCII, length-delimited, and is never truncated.
struct RobotIdentityResponse {
    uint8_t mode = ToWireValue(HostServerMode::IdentityQuery);
    uint8_t accepted = 0;
    uint16_t session_id = 0;
    uint8_t protocol_rev = 1;
    uint8_t model = 2;  // 0=BPX, 1=BPX-Pro, 2=Unknown, 3=BPW.
    uint8_t serial_number_length = 0;
    uint8_t reserved0 = 0;
    char serial_number[32] = {};
    uint8_t reserved[56] = {};  // Future fields; zero when sent, ignored in revision 1.
};
static_assert(sizeof(RobotIdentityResponse) == 96, "Identity response size mismatch");
static_assert(alignof(RobotIdentityResponse) == 2, "Identity response alignment mismatch");
static_assert(offsetof(RobotIdentityResponse, session_id) == 2, "Identity session offset mismatch");
static_assert(offsetof(RobotIdentityResponse, serial_number) == 8, "Identity SN offset mismatch");
static_assert(offsetof(RobotIdentityResponse, reserved) == 40, "Identity extension offset mismatch");
}  // namespace bpx_protocol
#endif
