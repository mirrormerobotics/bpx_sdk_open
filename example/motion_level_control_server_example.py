import argparse
import math
from datetime import datetime
import socket
import sys
import threading
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import bpx_sdk
from example_feedback import format_control_mode, format_power_state, print_identity_availability
from request_robot_state_example import print_robot_version


DEFAULT_LISTEN_PORT = 50051


def _uint16(text):
    try:
        value = int(text, 10)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("expected an integer") from exc
    if value < 0 or value > 65535:
        raise argparse.ArgumentTypeError("expected a value in range 0..65535")
    return value


def _positive_uint16(text):
    value = _uint16(text)
    if value == 0:
        raise argparse.ArgumentTypeError("expected a value in range 1..65535")
    return value


def parse_args():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Commands: zero, stand, tumbler, sit, charging-sit, damping, stop, status,\n"
            "          velocity X Y YAW, velocity-control on|off,\n"
            "          gait walk|running|bipedal|inv_bipedal|pronk|pace|bound|left_flip|right_flip,\n"
            "          jump up|front|back|left|right, quit"
        ),
    )
    parser.add_argument("--robot-ip", default=bpx_sdk.DEFAULT_SERVER_IP)
    parser.add_argument(
        "--state-port",
        "--robot-state-port",
        dest="robot_state_port",
        type=_uint16,
        default=bpx_sdk.DEFAULT_CLIENT_ROBOT_STATE_UDP_PORT,
    )
    parser.add_argument("--tcp-local-port", type=_uint16, default=0)
    parser.add_argument("--state-rate", dest="state_rate_hz", type=_positive_uint16, default=100)
    parser.add_argument("--listen-ip", default="127.0.0.1")
    parser.add_argument("--listen-port", type=_positive_uint16, default=DEFAULT_LISTEN_PORT)
    return parser.parse_args()


def motion_state_name(value):
    try:
        return bpx_sdk.MotionState(value).name
    except ValueError:
        return "Unknown"


def motion_gait_name(value):
    try:
        return bpx_sdk.MotionGait(value).name
    except ValueError:
        return "Unknown"


def status_line(motion):
    state = motion.getCurrentMotionState()
    last_state = motion.getLastMotionState()
    gait = motion.getCurrentGait()
    velocity = motion.getMaxVelocity()
    sn = motion.getRobotSerialNumber()
    model = motion.getRobotModel()
    model_name = {0: "Unknown", 1: "BPX", 2: "BPX-Pro", 3: "BPW"}.get(model, "Unknown")
    return (
        f"OK connected={int(motion.isConnected())} "
        f"SN={sn if sn is not None else '<unavailable>'} "
        f"model={model_name if model is not None else '<unavailable>'} "
        f"control_mode={format_control_mode(motion)} "
        f"motion_state={str(state) + '(' + motion_state_name(state) + ')' if state is not None else '<unavailable>'} "
        f"last_motion_state={str(last_state) + '(' + motion_state_name(last_state) + ')' if last_state is not None else '<unavailable>'} "
        f"gait={str(gait) + '(' + motion_gait_name(gait) + ')' if gait is not None else '<unavailable>'} "
        f"max_vel={velocity if velocity is not None else '<unavailable>'} "
        f"{format_power_state(motion)}"
    )


def print_telemetry_loop(motion, sdk_lock, stop_event):
    while not stop_event.is_set():
        with sdk_lock:
            state = status_line(motion)[3:]
        timestamp = datetime.now().isoformat(sep=" ", timespec="milliseconds")
        print(f"robot state: {timestamp} {state}", flush=True)
        stop_event.wait(1.0)


def execute_command(motion, sdk_lock, line):
    words = line.strip().split()
    if not words:
        return "ERR empty command", False

    command = words[0]
    if command == "help":
        return (
            "OK commands: zero, stand, tumbler, sit, charging-sit, damping, stop, status, "
            "velocity X Y YAW, velocity-control on|off, gait walk|running|bipedal|inv_bipedal|pronk|pace|bound|left_flip|right_flip, jump up|front|back|left|right, help, quit|shutdown",
            False,
        )

    with sdk_lock:
        return execute_motion_command(motion, words)


def execute_motion_command(motion, words):
    command = words[0]
    if command == "zero":
        motion.setZeroPositionsFlag()
        return "OK zero-position flag sent", False
    if command == "stand":
        return ("OK stand command sent" if motion.setStandUp() else "ERR stand command failed"), False
    if command == "tumbler":
        return ("OK tumbler command sent" if motion.setTumbler() else "ERR tumbler command failed"), False
    if command == "sit":
        return ("OK sit command sent" if motion.setSitDown() else "ERR sit command failed"), False
    if command == "charging-sit":
        return ("OK charging sit-down command sent" if motion.setChargingSitDown()
                else "ERR charging sit-down command failed"), False
    if command == "damping":
        return ("OK damping command sent" if motion.setDamping() else "ERR damping command failed"), False
    if command == "stop":
        ok = motion.setVelocity(0.0, 0.0, 0.0)
        motion.setVelocityControlFlag(False)
        return ("OK velocity stopped" if ok else "ERR stop command failed"), False
    if command == "status":
        return status_line(motion), False
    if command == "velocity":
        if len(words) != 4:
            return "ERR usage: velocity X Y YAW", False
        try:
            x, y, yaw = (float(words[1]), float(words[2]), float(words[3]))
        except ValueError:
            return "ERR velocity values must be numbers", False
        if not all(math.isfinite(value) for value in (x, y, yaw)):
            return "ERR velocity values must be finite numbers", False
        motion.setVelocityControlFlag(True)
        return ("OK velocity command sent" if motion.setVelocity(x, y, yaw) else "ERR velocity command failed"), False
    if command == "velocity-control":
        if len(words) != 2 or words[1] not in ("on", "off"):
            return "ERR usage: velocity-control on|off", False
        motion.setVelocityControlFlag(words[1] == "on")
        return f"OK velocity control {words[1]}", False
    if command == "jump":
        handlers = {"up": motion.setUpJump, "front": motion.setFrontJump,
                    "back": motion.setBackJump, "left": motion.setLeftJump,
                    "right": motion.setRightJump}
        if len(words) != 2 or words[1] not in handlers:
            return "ERR usage: jump up|front|back|left|right", False
        handlers[words[1]]()
        return "OK jump command sent", False
    if command == "gait":
        if len(words) != 2:
            return "ERR usage: gait NAME", False
        gait_handlers = {
            "walk": motion.setWalk,
            "running": motion.setRunning,
            "bipedal": motion.setBipedal,
            "inv_bipedal": motion.setInvBipedal,
            "pronk": motion.setPronk,
            "pace": motion.setPace,
            "bound": motion.setBound,
            "left_flip": motion.setLeftFlip,
            "right_flip": motion.setRightFlip,
        }
        handler = gait_handlers.get(words[1])
        if handler is None:
            return "ERR unknown gait", False
        handler()
        return "OK gait command sent", False
    if command in ("quit", "shutdown"):
        return "OK server shutting down", True
    return "ERR unknown command", False


def serve_client(conn, motion, sdk_lock):
    conn.sendall(b"OK motion-level server ready\n")
    should_stop = False
    with conn.makefile("r", encoding="utf-8", newline="\n") as reader:
        for line in reader:
            response, should_stop = execute_command(motion, sdk_lock, line)
            conn.sendall((response + "\n").encode("utf-8"))
            if should_stop:
                break
    return should_stop


def main():
    options = parse_args()

    print(f"bpx_sdk version={getattr(bpx_sdk, '__version__', 'unknown')}")
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

    if not motion.connect():
        raise RuntimeError("failed to connect motion level control")

    print_robot_version(motion)
    print_identity_availability(motion)
    sdk_lock = threading.Lock()
    stop_event = threading.Event()
    telemetry_thread = threading.Thread(
        target=print_telemetry_loop,
        args=(motion, sdk_lock, stop_event),
        daemon=True,
    )

    try:
        print("motion level control started")
        telemetry_thread.start()
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server.bind((options.listen_ip, options.listen_port))
            server.listen(4)
            print(f"waiting for clients on {options.listen_ip}:{options.listen_port}")

            should_stop = False
            while not should_stop:
                conn, _ = server.accept()
                with conn:
                    print("client connected")
                    try:
                        should_stop = serve_client(conn, motion, sdk_lock)
                    except (ConnectionError, UnicodeError) as exc:
                        print(f"client disconnected: {exc}")
                    print("client disconnected")
    finally:
        stop_event.set()
        if telemetry_thread.is_alive():
            telemetry_thread.join()
        with sdk_lock:
            motion.disconnect()


if __name__ == "__main__":
    main()
