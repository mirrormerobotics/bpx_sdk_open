"""Formatting shared by the robot state and control examples."""
import bpx_sdk


def print_identity_availability(robot):
    sn = robot.getRobotSerialNumber()
    model = robot.getRobotModel()
    # Successful identity queries are already printed by the SDK in connect().
    if sn is None or model is None:
        print("robot identity: <unavailable>", flush=True)


def format_control_mode(robot):
    mode = robot.getControlMode()
    if mode is None:
        return "<unavailable>"
    return f"{bpx_sdk.ControlMode(mode).name} ({mode})"


def format_power_state(robot):
    level = robot.getBatteryLevel()
    current = robot.getBatteryCurrent()
    in1 = robot.getChargerIn1()
    in2 = robot.getChargerIn2()
    estop = robot.getHardEstop()
    faults = robot.getFaultState()
    level_text = f"{level}%" if level is not None else "<unavailable>"
    current_text = f"{current:.3f}" if current is not None else "<unavailable>"
    if faults is None:
        fault_text = "<unavailable>"
    else:
        fault_text = f"{faults['entry_count']}{'+' if faults['truncated'] else ''}"
        if faults["entries"]:
            fault_text += f" top=0x{faults['entries'][0]['code']:x}"
    return (f"battery_level={level_text} battery_current={current_text} "
            f"charger_in1={in1 if in1 is not None else '<unavailable>'} "
            f"charger_in2={in2 if in2 is not None else '<unavailable>'} "
            f"hard_estop={estop if estop is not None else '<unavailable>'} "
            f"faults={fault_text}")
