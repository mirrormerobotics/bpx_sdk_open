import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import bpx_sdk
from example_options import parse_options


def print_version():
    print(f"bpx_sdk version={getattr(bpx_sdk, '__version__', 'unknown')}")


def main():
    options = parse_options(False)

    print_version()

    robot_state = bpx_sdk.RequestRobotState()
    robot_state.setRobotIp(options.robot_ip)
    robot_state.setRobotStateUploadPort(options.robot_state_port)
    robot_state.setTcpLocalPort(options.tcp_local_port)
    robot_state.setRobotStateUploadRate(options.state_rate_hz)

    print(
        f"robot_ip={options.robot_ip} "
        f"state_port={options.robot_state_port} "
        f"tcp_local_port={options.tcp_local_port} "
        f"state_rate={options.state_rate_hz}"
    )

    if not robot_state.connect():
        raise RuntimeError("failed to connect request robot state")

    try:
        while True:
            joint_pos = robot_state.getJointPosition()
            rpy = robot_state.getImuRpy()
            vel_body = robot_state.getCurrentVelocityBody()
            leg_odom = robot_state.getLegOdom()
            max_vel = robot_state.getMaxVelocity()
            battery = robot_state.getBatteryLevel()

            print("robot state:")
            if joint_pos is not None:
                print(f"  joint_pos[0]={joint_pos[0]:.3f}")
            if rpy is not None:
                print(f"  rpy=({rpy[0]:.3f}, {rpy[1]:.3f}, {rpy[2]:.3f})")
            if vel_body is not None:
                print(
                    f"  vel_body=({vel_body[0]:.3f}, "
                    f"{vel_body[1]:.3f}, {vel_body[2]:.3f})"
                )
            if leg_odom is not None:
                print(
                    f"  leg_odom=({leg_odom[0]:.3f}, "
                    f"{leg_odom[1]:.3f}, {leg_odom[2]:.3f})"
                )
            if max_vel is not None:
                print(
                    f"  max_vel=({max_vel[0]:.3f}, "
                    f"{max_vel[1]:.3f}, {max_vel[2]:.3f})"
                )
            if battery is not None:
                print(f"  battery_level={battery}%")

            time.sleep(0.2)
    finally:
        robot_state.disconnect()


if __name__ == "__main__":
    main()
