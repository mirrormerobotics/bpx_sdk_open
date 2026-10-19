import argparse
import socket
import sys


DEFAULT_SERVER_PORT = 50051


def _positive_uint16(text):
    try:
        value = int(text, 10)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("expected an integer") from exc
    if value <= 0 or value > 65535:
        raise argparse.ArgumentTypeError("expected a value in range 1..65535")
    return value


def parse_args():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Commands:\n"
            "  help                         List all commands and gait/jump choices\n"
            "  zero                         Send the zero-position flag\n"
            "  stand                        Stand up\n"
            "  tumbler                      Request self-righting recovery while lying down\n"
            "  sit                          Sit down\n"
            "  charging-sit                 Request charging sit-down\n"
            "  damping                      Enter damping mode\n"
            "  stop                         Set velocity to zero and disable velocity control\n"
            "  status                       Query connection, SN, model, control mode,\n"
            "                               current/previous motion state, gait, max velocity,\n"
            "                               battery, chargers, and hard estop\n"
            "  velocity X Y YAW             Set forward/lateral velocity (m/s) and yaw rate\n"
            "                               (rad/s), and enable velocity control\n"
            "  velocity-control on|off      Enable or disable velocity control\n"
            "  gait NAME                    Select a gait:\n"
            "                               walk, running, bipedal, inv_bipedal, pronk,\n"
            "                               pace, bound, left_flip, right_flip\n"
            "  jump up|front|back|left|right Select a jump direction (BPX)\n"
            "  quit, shutdown               Shut down the server\n"
            "\n"
            "Available actions depend on the robot model.\n"
            "Without a command, enter interactive mode; type help to list commands.\n"
            "Ctrl-D (Windows: Ctrl-Z then Enter) disconnects the client; quit stops the server.\n"
        ),
    )
    parser.add_argument("--server-ip", default="127.0.0.1", help="server IP (default: 127.0.0.1)")
    parser.add_argument("--server-port", type=_positive_uint16, default=DEFAULT_SERVER_PORT, help="server port (default: 50051)")
    parser.add_argument("command", nargs=argparse.REMAINDER, help="command and arguments; omit for interactive mode")
    return parser.parse_args()


def send_command(sock, reader, command):
    sock.sendall((command + "\n").encode("utf-8"))
    response = reader.readline()
    if not response:
        raise RuntimeError("server closed the connection")
    response = response.rstrip("\r\n")
    print(response)
    return response.startswith("OK")


def main():
    options = parse_args()
    command = options.command
    if command and command[0] == "--":
        command = command[1:]

    ok = True
    with socket.create_connection((options.server_ip, options.server_port)) as sock:
        reader = sock.makefile("r", encoding="utf-8", newline="\n")
        greeting = reader.readline()
        if greeting:
            print(greeting.rstrip("\r\n"))

        if command:
            ok = send_command(sock, reader, " ".join(command))
        else:
            print("Enter commands, help for command list, Ctrl-D to disconnect.")
            for line in sys.stdin:
                line = line.strip()
                if not line:
                    continue
                ok = send_command(sock, reader, line)
                if not ok or line in ("quit", "shutdown"):
                    break

    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
