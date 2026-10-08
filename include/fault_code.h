#ifndef BPX_SDK_FAULT_CODE_H_
#define BPX_SDK_FAULT_CODE_H_

#include <array>
#include <cstddef>
#include <cstdint>

namespace bpx_sdk {

// 64 位故障码位域（与运动主机 FaultCodeManager、dog_fault.proto 一致）：
//  63    60 59        40 39    36 35    32 31      24 23      16 15              0
//  ┌────────┬─────────────┬────────┬────────┬──────────┬──────────┬────────────────┐
//  │Version │  Reserved   │  Sep   │Severity│ ModuleID │ SubModule│   SubCode      │
//  └────────┴─────────────┴────────┴────────┴──────────┴──────────┴────────────────┘
constexpr uint64_t kFaultCodeVersionShift = 60;
constexpr uint64_t kFaultCodeSepShift = 36;
constexpr uint64_t kFaultCodeSeverityShift = 32;
constexpr uint64_t kFaultCodeModuleShift = 24;
constexpr uint64_t kFaultCodeSubModuleShift = 16;
constexpr uint64_t kFaultCodeVersionMask = 0xFu;
constexpr uint64_t kFaultCodeSepMask = 0xFu;
constexpr uint64_t kFaultCodeSeverityMask = 0xFu;
constexpr uint64_t kFaultCodeModuleMask = 0xFFu;
constexpr uint64_t kFaultCodeSubModuleMask = 0xFFu;
constexpr uint64_t kFaultCodeSubCodeMask = 0xFFFFu;

// 严重等级，对应故障码 [35:32]。只列出当前固件实际会打包的档位。
enum FaultSeverity : uint8_t {
    kFaultSeverityWarning = 0x1,
    kFaultSeverityError = 0x2,
};

// 故障来源模块，对应故障码 [31:24]。只列出当前固件实际会报告的模块。
enum FaultModule : uint8_t {
    kFaultModuleJoint = 0x02,    // 电机 Motor
    kFaultModuleDriver = 0x03,   // 电机驱动器 Motor Driver
    kFaultModuleImu = 0x04,      // 陀螺仪 IMU
    kFaultModuleBattery = 0x05,  // 电池管理 Battery Management System
};

inline uint8_t DecodeFaultVersion(uint64_t code) {
    return static_cast<uint8_t>((code >> kFaultCodeVersionShift) & kFaultCodeVersionMask);
}

inline uint8_t DecodeFaultSeverity(uint64_t code) {
    return static_cast<uint8_t>((code >> kFaultCodeSeverityShift) & kFaultCodeSeverityMask);
}

inline uint8_t DecodeFaultModule(uint64_t code) {
    return static_cast<uint8_t>((code >> kFaultCodeModuleShift) & kFaultCodeModuleMask);
}

inline uint8_t DecodeFaultSubModule(uint64_t code) {
    return static_cast<uint8_t>((code >> kFaultCodeSubModuleShift) & kFaultCodeSubModuleMask);
}

inline uint16_t DecodeFaultSubCode(uint64_t code) {
    return static_cast<uint16_t>(code & kFaultCodeSubCodeMask);
}

// 1 Hz 故障快照通道（UDP head=0x00F0）的最大条数；活动故障超过该数量时
// 机器人置 truncated=true 并保留 severity 最高的前 32 条。
constexpr size_t kMaxFaultEntries = 32;

// 单条活动故障。occurred_t_ms 为 Unix epoch 毫秒；与状态包头的 time_ms
// （单调时钟）分属不同时钟域，不可互减。
struct RobotFaultEntry {
    uint64_t code = 0;
    uint64_t occurred_t_ms = 0;
};

// 全量活动故障快照。entries 按 severity 降序、同 severity 按打包码升序
// 排列，entries[0] 即当前最高优先级故障（等价于 primary fault）；无故障时
// entry_count=0，机器人仍以 1 Hz 持续发送空快照作为通道心跳。
struct RobotFaultState {
    uint32_t entry_count = 0;
    bool truncated = false;
    std::array<RobotFaultEntry, kMaxFaultEntries> entries{};
};

}  // namespace bpx_sdk

#endif  // BPX_SDK_FAULT_CODE_H_
