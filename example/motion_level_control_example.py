import sys
import time
from enum import Enum
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import bpx_sdk
from example_options import parse_options


class DemoPhase(Enum):
    WAIT = 0
    STAND_UP = 1
    YAW_VELOCITY = 2
    ZERO_VELOCITY = 3
    GAIT_INPUT = 4
    SIT_DOWN = 5


def print_version():
    print(f"bpx_sdk version={getattr(bpx_sdk, '__version__', 'unknown')}")


def main():
    options = parse_options(False)

    print_version()

    motion_level_control = bpx_sdk.MotionLevelControl()

    # Set the target robot IP.
    motion_level_control.setRobotIp(options.robot_ip)

    # Set the local port for receiving robot state packets.
    motion_level_control.setRobotStateUploadPort(options.robot_state_port)

    # Set the local control connection port. Use 0 for automatic selection.
    motion_level_control.setTcpLocalPort(options.tcp_local_port)

    # Set the requested robot state upload rate.
    motion_level_control.setRobotStateUploadRate(options.state_rate_hz)

    # Set the motion command send rate and velocity control mode.
    motion_level_control.setMotionCommandRate(50)

    print(
        f"robot_ip={options.robot_ip} "
        f"state_port={options.robot_state_port} "
        f"tcp_local_port={options.tcp_local_port} "
        f"state_rate={options.state_rate_hz}"
    )

    if not motion_level_control.connect():
        raise RuntimeError("failed to connect motion level control")

    try:
        print("motion level control running")
        phase = DemoPhase.WAIT
        phase_start = time.monotonic()
        phase_announced = False

        while True:
            now = time.monotonic()
            phase_elapsed = now - phase_start

            current_state = motion_level_control.getCurrentMotionState()
            current_gait = motion_level_control.getCurrentGait()
            max_vel = motion_level_control.getMaxVelocity()
            has_motion_state = (
                current_state is not None and
                current_gait is not None and
                max_vel is not None
            )

            if has_motion_state:
                print(
                    f"motion_state={current_state} gait={current_gait} "
                    f"max_vel=({max_vel[0]}, {max_vel[1]}, {max_vel[2]})"
                )

            if phase == DemoPhase.WAIT:
                if not phase_announced:
                    motion_level_control.setZeroPositionsFlag()
                    print("send zero-position command")
                    print("wait 1 second before sending commands")
                    phase_announced = True

                if phase_elapsed >= 1.0:
                    phase = DemoPhase.STAND_UP
                    phase_start = now
                    phase_announced = False

            elif phase == DemoPhase.STAND_UP:
                if not phase_announced:
                    print("send stand-up command")
                    phase_announced = True

                if not motion_level_control.setStandUp():
                    raise RuntimeError("failed to send stand-up command")

                if has_motion_state and current_state == bpx_sdk.MotionState.Motion:
                    phase = DemoPhase.YAW_VELOCITY
                    phase_start = now
                    phase_announced = False
                    motion_level_control.setVelocityControlFlag(True)

            elif phase == DemoPhase.YAW_VELOCITY:
                if not phase_announced:
                    print("send yaw velocity command for 3 seconds")
                    phase_announced = True

                if not motion_level_control.setVelocity(0.0, 0.0, 1.0):
                    raise RuntimeError("failed to send yaw velocity command")

                if phase_elapsed >= 3.0:
                    phase = DemoPhase.ZERO_VELOCITY
                    phase_start = now
                    phase_announced = False
                    motion_level_control.setVelocityControlFlag(False)

            elif phase == DemoPhase.ZERO_VELOCITY:
                if not phase_announced:
                    print("send zero velocity command for 2 seconds")
                    phase_announced = True

                if not motion_level_control.setVelocity(0.0, 0.0, 0.0):
                    raise RuntimeError("failed to send zero velocity command")

                if phase_elapsed >= 2.0:
                    phase = DemoPhase.GAIT_INPUT
                    # phase = DemoPhase.SIT_DOWN
                    phase_start = now
                    phase_announced = False

            elif phase == DemoPhase.GAIT_INPUT:
                if not phase_announced:
                    print("send left-flip command for 2 seconds")
                    phase_announced = True
                    # motion_level_control.setLeftFlip()
                    # motion_level_control.setRightFlip()
                    # motion_level_control.setBound()
                    # motion_level_control.setInvBipedal()

                if phase_elapsed >= 1.0:
                    motion_level_control.setWalk()
                    phase = DemoPhase.SIT_DOWN
                    phase_start = now
                    phase_announced = False

            elif phase == DemoPhase.SIT_DOWN:
                if not phase_announced:
                    print("send sit-down command, exit after 1 second")
                    phase_announced = True

                if not motion_level_control.setSitDown():
                    raise RuntimeError("failed to send sit-down command")

                if phase_elapsed >= 5.0:
                    return

            time.sleep(0.2)
    finally:
        motion_level_control.disconnect()


if __name__ == "__main__":
    main()
