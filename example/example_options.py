import argparse
from dataclasses import dataclass

import bpx_sdk


@dataclass
class Options:
    robot_ip: str
    robot_state_port: int
    joint_state_port: int
    tcp_local_port: int
    state_rate_hz: int


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


def parse_options(enable_joint_state_port=False):
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--robot-ip",
        default=bpx_sdk.DEFAULT_SERVER_IP,
        help=f"Robot IP address (default: {bpx_sdk.DEFAULT_SERVER_IP})",
    )
    parser.add_argument(
        "--state-port",
        "--robot-state-port",
        dest="robot_state_port",
        type=_uint16,
        default=bpx_sdk.DEFAULT_CLIENT_ROBOT_STATE_UDP_PORT,
        help=(
            "Local UDP port for robot state "
            f"(default: {bpx_sdk.DEFAULT_CLIENT_ROBOT_STATE_UDP_PORT})"
        ),
    )
    parser.add_argument(
        "--tcp-local-port",
        type=_uint16,
        default=0,
        help="Local TCP port, 0 for automatic selection (default: 0)",
    )
    parser.add_argument(
        "--state-rate",
        dest="state_rate_hz",
        type=_positive_uint16,
        default=100,
        help="Requested robot state upload rate (default: 100)",
    )
    if enable_joint_state_port:
        parser.add_argument(
            "--joint-state-port",
            type=_uint16,
            default=bpx_sdk.DEFAULT_CLIENT_JOINT_STATE_UDP_PORT,
            help=(
                "Local UDP port for high-rate joint state "
                f"(default: {bpx_sdk.DEFAULT_CLIENT_JOINT_STATE_UDP_PORT})"
            ),
        )
    else:
        parser.set_defaults(joint_state_port=bpx_sdk.DEFAULT_CLIENT_JOINT_STATE_UDP_PORT)

    args = parser.parse_args()
    return Options(
        robot_ip=args.robot_ip,
        robot_state_port=args.robot_state_port,
        joint_state_port=args.joint_state_port,
        tcp_local_port=args.tcp_local_port,
        state_rate_hz=args.state_rate_hz,
    )
