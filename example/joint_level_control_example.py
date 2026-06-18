import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import bpx_sdk
from example_options import parse_options


def print_version():
    print(f"bpx_sdk version={getattr(bpx_sdk, '__version__', 'unknown')}")


def format_values(values, count=6):
    return ", ".join(f"{values[i]:.3f}" for i in range(count))


def print_robot_status(joint_level_control):
    current_state = joint_level_control.getCurrentMotionState()
    current_gait = joint_level_control.getCurrentGait()
    battery_level = joint_level_control.getBatteryLevel()
    joint_timestamp = joint_level_control.getJointStateTimestampHighRate()
    joint_seq = joint_level_control.getJointStateSeqHighRate()
    rpy = joint_level_control.getImuRpyHighRate()
    joint_pos = joint_level_control.getJointPositionHighRate()
    joint_vel = joint_level_control.getJointVelocityHighRate()
    joint_tau = joint_level_control.getJointTorqueHighRate()

    parts = ["[state]"]
    if current_state is not None and current_gait is not None:
        parts.append(f"motion={current_state} gait={current_gait}")
    if battery_level is not None:
        parts.append(f"battery={battery_level}%")
    if joint_timestamp is not None and joint_seq is not None:
        parts.append(f"high_rate_seq={joint_seq} high_rate_t={joint_timestamp:.3f}")
    if rpy is not None:
        parts.append(f"rpy=({rpy[0]:.3f}, {rpy[1]:.3f}, {rpy[2]:.3f})")
    if joint_pos is not None and joint_vel is not None and joint_tau is not None:
        parts.append(f"q[0..5]=({format_values(joint_pos)})")
        parts.append(f"dq[0..5]=({format_values(joint_vel)})")
        parts.append(f"tau[0..5]=({format_values(joint_tau)})")

    print(" ".join(parts))


def make_leg_target(abad, hip, knee):
    target = [0.0] * bpx_sdk.JOINT_COUNT
    for leg in range(4):
        target[3 * leg + 0] = abad
        target[3 * leg + 1] = hip
        target[3 * leg + 2] = knee
    return target


def send_position_command(joint_level_control, pos):
    return joint_level_control.setJointPosition(pos)


def run_hold_stage(joint_level_control, hold_pos, duration, next_status_print):
    status_print_period = 0.2
    start = time.monotonic()

    while time.monotonic() - start < duration:
        if not send_position_command(joint_level_control, hold_pos):
            raise RuntimeError("failed to send hold joint command")

        now = time.monotonic()
        if now >= next_status_print:
            print_robot_status(joint_level_control)
            next_status_print = now + status_print_period

        time.sleep(0.01)

    return next_status_print


def run_move_stage(joint_level_control, start_pos, target_pos, duration, next_status_print):
    status_print_period = 0.2
    start = time.monotonic()
    cmd_pos = [0.0] * bpx_sdk.JOINT_COUNT

    while True:
        elapsed = time.monotonic() - start
        alpha = min(elapsed / duration, 1.0)

        for i in range(bpx_sdk.JOINT_COUNT):
            cmd_pos[i] = start_pos[i] + alpha * (target_pos[i] - start_pos[i])

        if not send_position_command(joint_level_control, cmd_pos):
            raise RuntimeError("failed to send moving joint command")

        now = time.monotonic()
        if now >= next_status_print:
            print_robot_status(joint_level_control)
            next_status_print = now + status_print_period

        if alpha >= 1.0:
            break

        time.sleep(0.01)

    return next_status_print


def main():
    options = parse_options(True)

    print_version()

    joint_level_control = bpx_sdk.JointLevelControl()

    # Set the robot address.
    joint_level_control.setRobotIp(options.robot_ip)

    # Set the local state receiving port.
    joint_level_control.setRobotStateUploadPort(options.robot_state_port)
    joint_level_control.setJointStateUploadPort(options.joint_state_port)

    # Use 0 to let the system choose the local connection port automatically.
    joint_level_control.setTcpLocalPort(options.tcp_local_port)

    # Set the requested robot state upload rate.
    joint_level_control.setRobotStateUploadRate(options.state_rate_hz)

    print(
        f"robot_ip={options.robot_ip} "
        f"state_port={options.robot_state_port} "
        f"joint_state_port={options.joint_state_port} "
        f"tcp_local_port={options.tcp_local_port} "
        f"state_rate={options.state_rate_hz}"
    )

    if not joint_level_control.connect():
        raise RuntimeError("failed to connect joint level control")

    try:
        init_pos = None
        while init_pos is None:
            init_pos = joint_level_control.getJointPositionHighRate()
            if init_pos is None:
                print("waiting for initial high-rate joint position...")
                time.sleep(0.1)

        first_target_pos = make_leg_target(0.0, 0.8, -1.8)
        second_target_pos = make_leg_target(0.0, 1.2, -2.7)

        kp = [100.0] * bpx_sdk.JOINT_COUNT
        kd = [2.0] * bpx_sdk.JOINT_COUNT
        vel = [0.0] * bpx_sdk.JOINT_COUNT
        tff = [0.0] * bpx_sdk.JOINT_COUNT
        if not joint_level_control.setJointCommand(kp, init_pos, kd, vel, tff):
            raise RuntimeError("failed to set initial joint gains")

        next_status_print = time.monotonic()
        hold_duration = 1.0
        move_duration = 2.0

        print("hold current joint position for 1s")
        next_status_print = run_hold_stage(
            joint_level_control,
            init_pos,
            hold_duration,
            next_status_print,
        )

        print("move to first target position in 2s")
        next_status_print = run_move_stage(
            joint_level_control,
            init_pos,
            first_target_pos,
            move_duration,
            next_status_print,
        )

        print("hold first target position for 1s")
        next_status_print = run_hold_stage(
            joint_level_control,
            first_target_pos,
            hold_duration,
            next_status_print,
        )

        print("move every leg to (0, 1.2, -2.7) in 2s")
        next_status_print = run_move_stage(
            joint_level_control,
            first_target_pos,
            second_target_pos,
            move_duration,
            next_status_print,
        )

        print("hold final target position for 1s")
        run_hold_stage(
            joint_level_control,
            second_target_pos,
            hold_duration,
            next_status_print,
        )

        joint_level_control.setZeroJointCommand()
    finally:
        joint_level_control.disconnect()


if __name__ == "__main__":
    main()
