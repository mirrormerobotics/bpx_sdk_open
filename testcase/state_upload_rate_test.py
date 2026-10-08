#!/usr/bin/env python3
"""Measure BPX ordinary UDP state upload rates without using SDK caches.

The tool opens the existing 24-byte TCP state subscription, receives the
32-byte subscription responses, and counts every ordinary UDP state packet.
It never sends motion-level or joint-level commands.
"""

import argparse
import select
import socket
import struct
import sys
import time
from dataclasses import dataclass


DEFAULT_ROBOT_IP = "10.21.20.1"
DEFAULT_SERVER_PORT = 10860
DEFAULT_STATE_PORT = 9873
DEFAULT_JOINT_STATE_PORT = 7895

STATE_QUERY_MODE = 1
PROTOCOL_VERSION = 1
SESSION_ID = 1

SUBSCRIBE_REQUEST = struct.Struct("<HHHHHBxIII")
SUBSCRIBE_RESPONSE = struct.Struct("<HBxHBBIHHIIII")
UDP_HEADER = struct.Struct("<HHI")


@dataclass(frozen=True)
class Group:
    name: str
    header: int
    payload_size: int
    rate_hz: int
    configurable: bool


GROUPS = (
    Group("joint", 0x1000, 144, 1000, True),
    Group("imu", 0x0200, 52, 200, True),
    Group("leg_odom", 0x0050, 52, 10, False),
    Group("motion", 0x0010, 20, 5, False),
    Group("battery_temp", 0x0001, 32, 1, False),
    Group("fault", 0x00F0, 520, 1, False),
)
GROUP_BY_HEADER = {group.header: group for group in GROUPS}


def expected_group_rate(group, requested_rate_hz):
    return min(requested_rate_hz, group.rate_hz) if group.configurable else group.rate_hz


def uint16(text):
    value = int(text, 10)
    if value < 0 or value > 65535:
        raise argparse.ArgumentTypeError("expected a value in range 0..65535")
    return value


def positive_float(text):
    value = float(text)
    if value <= 0:
        raise argparse.ArgumentTypeError("expected a value greater than zero")
    return value


def build_request(state_port, joint_state_port, rate_hz):
    request_time_ms = int(time.monotonic() * 1000) & 0xFFFFFFFF
    return SUBSCRIBE_REQUEST.pack(
        SESSION_ID,
        state_port,
        joint_state_port,
        PROTOCOL_VERSION,
        rate_hz,
        STATE_QUERY_MODE,
        request_time_ms,
        0,
        0,
    )


def parse_udp_packet(data):
    if len(data) < UDP_HEADER.size:
        return None, "short_header"
    header, payload_size, _time_ms = UDP_HEADER.unpack_from(data)
    group = GROUP_BY_HEADER.get(header)
    if group is None:
        return None, "unknown_header"
    if payload_size != group.payload_size:
        return None, "wrong_payload_size"
    if len(data) != UDP_HEADER.size + payload_size:
        return None, "wrong_packet_size"
    return group, None


class Measurement:
    def __init__(self, tcp_socket, udp_socket, robot_ip):
        self.tcp_socket = tcp_socket
        self.udp_socket = udp_socket
        self.robot_ip = robot_ip
        self.tcp_buffer = bytearray()
        self.accepted = False
        self.actual_rate_hz = 0
        self.first_udp_seen = False
        self.tcp_responses = 0
        self.wrong_source = 0
        self.invalid = {}

    def receive(self, timeout, counts=None):
        readable, _, _ = select.select(
            (self.tcp_socket, self.udp_socket), (), (), max(0.0, timeout)
        )
        for current in readable:
            if current is self.tcp_socket:
                data = current.recv(4096)
                if not data:
                    raise RuntimeError("robot closed the TCP subscription")
                self.tcp_buffer.extend(data)
                while len(self.tcp_buffer) >= SUBSCRIBE_RESPONSE.size:
                    frame = bytes(self.tcp_buffer[: SUBSCRIBE_RESPONSE.size])
                    del self.tcp_buffer[: SUBSCRIBE_RESPONSE.size]
                    values = SUBSCRIBE_RESPONSE.unpack(frame)
                    session_id, accepted, actual_rate_hz, mode = values[:4]
                    if session_id != SESSION_ID or mode != STATE_QUERY_MODE:
                        continue
                    self.tcp_responses += 1
                    self.accepted = bool(accepted)
                    self.actual_rate_hz = actual_rate_hz
            else:
                data, address = current.recvfrom(65535)
                if address[0] != self.robot_ip:
                    self.wrong_source += 1
                    continue
                group, error = parse_udp_packet(data)
                if error is not None:
                    self.invalid[error] = self.invalid.get(error, 0) + 1
                    continue
                self.first_udp_seen = True
                if counts is not None:
                    counts[group.name] += 1


def wait_until_ready(measurement, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        measurement.receive(min(0.2, deadline - time.monotonic()))
        if measurement.accepted and measurement.first_udp_seen:
            return
    missing = []
    if not measurement.accepted:
        missing.append("accepted TCP response")
    if not measurement.first_udp_seen:
        missing.append("first UDP state packet")
    raise RuntimeError("startup timeout waiting for " + " and ".join(missing))


def run_window(measurement, duration, counts=None):
    deadline = time.monotonic() + duration
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return
        measurement.receive(min(0.2, remaining), counts)


def evaluate(count, expected_rate_hz, duration, tolerance):
    expected_count = expected_rate_hz * duration
    allowed_count_error = max(expected_count * tolerance, 2.0 if expected_rate_hz < 10 else 1.0)
    passed = abs(count - expected_count) <= allowed_count_error
    measured_rate_hz = count / duration
    error_percent = (
        abs(measured_rate_hz - expected_rate_hz) / expected_rate_hz * 100.0
        if expected_rate_hz
        else 0.0
    )
    return passed, measured_rate_hz, error_percent


def self_test():
    assert SUBSCRIBE_REQUEST.size == 24
    assert SUBSCRIBE_RESPONSE.size == 32
    assert UDP_HEADER.size == 8
    request = build_request(9873, 7895, 100)
    assert len(request) == 24
    for group in GROUPS:
        packet = UDP_HEADER.pack(group.header, group.payload_size, 1234) + bytes(
            group.payload_size
        )
        parsed, error = parse_udp_packet(packet)
        assert error is None and parsed == group
    assert parse_udp_packet(b"bad")[1] == "short_header"
    assert evaluate(1000, 100, 10, 0.1)[0]
    assert not evaluate(700, 100, 10, 0.1)[0]
    expected_at_100 = {
        group.name: expected_group_rate(group, 100) for group in GROUPS
    }
    assert expected_at_100 == {
        "joint": 100,
        "imu": 100,
        "leg_odom": 10,
        "motion": 5,
        "battery_temp": 1,
    }
    assert expected_group_rate(GROUP_BY_HEADER[0x1000], 1000) == 1000
    assert expected_group_rate(GROUP_BY_HEADER[0x0200], 1000) == 200
    print("self-test: PASS")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Count real BPX ordinary UDP state packets by data group."
    )
    parser.add_argument("--robot-ip", default=DEFAULT_ROBOT_IP)
    parser.add_argument("--server-port", type=uint16, default=DEFAULT_SERVER_PORT)
    parser.add_argument("--state-port", type=uint16, default=DEFAULT_STATE_PORT)
    parser.add_argument(
        "--joint-state-port", type=uint16, default=DEFAULT_JOINT_STATE_PORT
    )
    parser.add_argument("--tcp-local-port", type=uint16, default=0)
    parser.add_argument("--state-rate", type=uint16, default=100)
    parser.add_argument("--duration", type=positive_float, default=10.0)
    parser.add_argument("--warmup", type=positive_float, default=2.0)
    parser.add_argument("--startup-timeout", type=positive_float, default=8.0)
    parser.add_argument("--tolerance", type=positive_float, default=0.10)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.state_port == 0:
        parser.error("--state-port must be in range 1..65535")
    if args.server_port == 0:
        parser.error("--server-port must be in range 1..65535")
    if args.state_rate == 0:
        parser.error("--state-rate must be in range 1..65535")
    return args


def main():
    args = parse_args()
    if args.self_test:
        self_test()
        return 0

    udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        udp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1 << 20)
        udp_socket.bind(("0.0.0.0", args.state_port))

        if args.tcp_local_port:
            tcp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            tcp_socket.bind(("0.0.0.0", args.tcp_local_port))
        tcp_socket.settimeout(args.startup_timeout)
        tcp_socket.connect((args.robot_ip, args.server_port))
        tcp_socket.sendall(
            build_request(args.state_port, args.joint_state_port, args.state_rate)
        )
        tcp_socket.setblocking(False)
        udp_socket.setblocking(False)

        measurement = Measurement(tcp_socket, udp_socket, args.robot_ip)
        print(
            f"subscribing robot={args.robot_ip}:{args.server_port} "
            f"udp_port={args.state_port} requested_rate={args.state_rate}Hz"
        )
        wait_until_ready(measurement, args.startup_timeout)
        print(
            f"subscription accepted actual_rate={measurement.actual_rate_hz}Hz; "
            f"warming up for {args.warmup:.1f}s"
        )
        run_window(measurement, args.warmup)

        counts = {group.name: 0 for group in GROUPS}
        print(f"measuring for {args.duration:.3f}s")
        started = time.monotonic()
        run_window(measurement, args.duration, counts)
        elapsed = time.monotonic() - started

        rate_cap = measurement.actual_rate_hz or args.state_rate
        all_passed = True
        print("\nGroup          Packets  Measured    Expected    Error    Result")
        print("-------------  -------  ----------  ----------  -------  ------")
        for group in GROUPS:
            expected_rate = expected_group_rate(group, rate_cap)
            passed, measured_rate, error_percent = evaluate(
                counts[group.name], expected_rate, elapsed, args.tolerance
            )
            all_passed = all_passed and passed
            print(
                f"{group.name:<13}  {counts[group.name]:>7}  "
                f"{measured_rate:>8.2f}Hz  {expected_rate:>8.2f}Hz  "
                f"{error_percent:>6.2f}%  {'PASS' if passed else 'FAIL'}"
            )

        invalid_total = sum(measurement.invalid.values())
        print(
            f"\nelapsed={elapsed:.3f}s tcp_responses={measurement.tcp_responses} "
            f"invalid_udp={invalid_total} wrong_source={measurement.wrong_source}"
        )
        if measurement.invalid:
            print("invalid_breakdown=" + repr(measurement.invalid))
        print("overall: " + ("PASS" if all_passed else "FAIL"))
        return 0 if all_passed else 2
    except (OSError, RuntimeError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    finally:
        tcp_socket.close()
        udp_socket.close()


if __name__ == "__main__":
    raise SystemExit(main())
