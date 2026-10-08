"""Automatically stand up, run four pose sweeps, then lie down; use --robot-ip IP."""
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import bpx_sdk
from example_options import parse_options

COMMAND_INTERVAL = 0.02
SWEEP_DURATION = 3.0
ENDPOINT_HOLD = 1.0
NEUTRAL_PAUSE = 1.0
POSTURE_TIMEOUT = 10.0
SWEEP_STEPS = round(SWEEP_DURATION / COMMAND_INTERVAL)


def sweep_pose_axis(motion, axis_index, axis_name):
    start = time.monotonic()
    for step in range(SWEEP_STEPS + 1):
        # Send -1000 immediately, hold it, then start the timed sweep.
        if step > 0:
            target = start + ENDPOINT_HOLD + step * COMMAND_INTERVAL
            time.sleep(max(0.0, target - time.monotonic()))
        value = -1000 + 2000 * step // SWEEP_STEPS
        axes = [0.0] * 4
        axes[axis_index] = value / 1000.0
        if not motion.setPose(*axes):
            raise RuntimeError('pose sweep command failed')
        display = [0] * 4
        display[axis_index] = value
        print(f'axis={axis_name} x={display[0]} y={display[1]} '
              f'z={display[2]} r={display[3]}', flush=True)
    # Hold +1000 before returning to neutral and starting the next axis.
    time.sleep(ENDPOINT_HOLD)
    if not motion.setPose(0.0, 0.0, 0.0, 0.0):
        raise RuntimeError('neutral pose command failed')
    print(f'axis={axis_name} complete x=0 y=0 z=0 r=0', flush=True)
    time.sleep(NEUTRAL_PAUSE)


def wait_until(ready, error, timeout=3.0):
    deadline = time.monotonic() + timeout
    while not ready():
        if time.monotonic() >= deadline:
            raise RuntimeError(error)
        time.sleep(0.02)


def main():
    options = parse_options(False)
    motion = bpx_sdk.MotionLevelControl()
    motion.setRobotIp(options.robot_ip)
    motion.setRobotStateUploadPort(options.robot_state_port)
    motion.setTcpLocalPort(options.tcp_local_port)
    motion.setRobotStateUploadRate(options.state_rate_hz)
    motion.setMotionCommandRate(50)
    print(
        f"robot_ip={options.robot_ip} "
        f"state_port={options.robot_state_port} "
        f"tcp_local_port={options.tcp_local_port} "
        f"state_rate={options.state_rate_hz}"
    )
    try:
        if not motion.connect():
            raise RuntimeError('connection failed')
        print('request stand-up', flush=True)
        wait_until(motion.setStandUp, 'stand-up request failed')
        wait_until(lambda: motion.getCurrentMotionState() == bpx_sdk.MotionState.Motion,
                   'robot did not finish standing up', POSTURE_TIMEOUT)
        print('stand-up complete; request pose tracking', flush=True)
        wait_until(motion.setPoseTracking, 'pose request failed')
        wait_until(lambda: motion.getCurrentGait() == bpx_sdk.MotionGait.PoseTracking,
                   'robot did not enter pose tracking')
        # Public API order is x, y, z, r.
        for axis_name, axis_index in [('x', 0), ('y', 1), ('z', 2), ('r', 3)]:
            sweep_pose_axis(motion, axis_index, axis_name)
        motion.setWalk()
        wait_until(lambda: motion.getCurrentGait() == bpx_sdk.MotionGait.Walk,
                   'robot did not return to walking gait')
        print('pose sweeps complete; request lying down', flush=True)
        wait_until(motion.setSitDown, 'lying-down request failed')
        wait_until(lambda: motion.getCurrentMotionState() == bpx_sdk.MotionState.LyingDown,
                   'robot did not finish lying down', POSTURE_TIMEOUT)
        print('lying-down complete', flush=True)
    finally:
        motion.setPose(0.0, 0.0, 0.0, 0.0)
        motion.disconnect()


if __name__ == '__main__':
    main()
