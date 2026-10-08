"""Check the real examples' UDP sweeps against a local peer, without robot hardware.

Set BPX_EXAMPLE_BUILD to a directory containing the C++ pose_tracking_example.
"""
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import time
import unittest

import test_python_robot_features as robot_features

ROOT = Path(__file__).resolve().parents[1]


class PoseTrackingExamplesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        robot_features.RobotFeaturesTest.setUpClass()

    @classmethod
    def tearDownClass(cls):
        robot_features.RobotFeaturesTest.tearDownClass()

    def test_axis_order_endpoints_and_neutral_pauses(self):
        commands = [[sys.executable, str(ROOT / 'example/pose_tracking_example.py')]]
        if os.environ.get('BPX_EXAMPLE_BUILD'):
            commands.append([str(Path(os.environ['BPX_EXAMPLE_BUILD']) / 'pose_tracking_example')])
        for command in commands:
            with self.subTest(command=command[0]):
                self.check_sweep(command)

    def check_sweep(self, command):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
            reservation.bind(('127.0.0.1', 0))
            state_port = reservation.getsockname()[1]
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp, \
                tempfile.TemporaryFile(mode='w+') as log:
            udp.bind(('127.0.0.1', 9527))
            udp.settimeout(0.1)
            process = subprocess.Popen(command + [
                '--robot-ip', '127.0.0.1', '--state-port', str(state_port),
                '--state-rate', '100'], stdout=log, stderr=subprocess.STDOUT)
            samples = {}
            robot_state = 0  # Start lying down, as on the robot.
            robot_gait = 0
            stand_started = None
            stand_finished = None
            sit_started = None
            lying_feedback_sent = None
            try:
                deadline = time.monotonic() + 40
                while process.poll() is None:
                    self.assertLess(time.monotonic(), deadline, 'example timed out')
                    try:
                        packet = udp.recv(1024)
                    except socket.timeout:
                        continue
                    now = time.monotonic()
                    self.assertEqual(len(packet), 56)
                    gait = packet[5]
                    mode = packet[4]
                    version = struct.unpack_from('<I', packet, 40)[0]
                    samples.setdefault(version, (now, gait, packet[32],
                        struct.unpack_from('<6f', packet, 8), mode))
                    if mode == 1 and stand_started is None:
                        stand_started = now
                        robot_state = 1
                    # Exercise completion waits longer than the old 3 s timeout.
                    if stand_started is not None and stand_finished is None and now - stand_started >= 3.2:
                        robot_state = 6
                        stand_finished = now
                    if gait == 7:
                        self.assertIsNotNone(stand_finished, 'pose selected before standing finished')
                        self.assertEqual(robot_state, 6)
                    if robot_state == 6:
                        robot_gait = gait
                    if mode == 3 and sit_started is None:
                        self.assertEqual(robot_state, 6)
                        sit_started = now
                        robot_state = 3
                    if sit_started is not None and now - sit_started >= 3.2:
                        robot_state = 0
                        if lying_feedback_sent is None:
                            lying_feedback_sent = now
                    # Return actual state progression, not just command targets.
                    payload = bytearray(20)
                    payload[0], payload[1] = robot_state, robot_gait
                    udp.sendto(struct.pack('<HHI', 0x10, 20, version + 1) + payload,
                               ('127.0.0.1', state_port))
                log.seek(0)
                output = log.read()
                self.assertEqual(process.returncode, 0, output)
                self.assertIsNotNone(lying_feedback_sent, 'example exited before lying-down feedback')
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=3)

        pose = [sample for _, sample in sorted(samples.items()) if sample[1] == 7]
        stand_commands = [sample for sample in samples.values() if sample[4] == 1]
        self.assertTrue(stand_commands, 'example never requested standing up')
        self.assertTrue(all(sample[3] == (0.0,) * 6 for sample in stand_commands))
        self.assertEqual(len(pose), 1 + 4 * 152, output)
        self.assertEqual(pose[0][3], (0.0,) * 6)  # Gait selection starts at neutral.
        expected_lines = []
        for number, (name, index) in enumerate([('x', 0), ('y', 1), ('z', 3), ('r', 2)]):
            sweep = pose[1 + number * 152:1 + (number + 1) * 152]
            # -1000 holds for 1 s; the first changed point follows at 1.02 s.
            self.assertAlmostEqual(sweep[1][0] - sweep[0][0], 1.02, delta=0.15)
            self.assertAlmostEqual(sweep[150][0] - sweep[0][0], 4.0, delta=0.15)
            self.assertAlmostEqual(sweep[151][0] - sweep[150][0], 1.0, delta=0.15)
            for step, sample in enumerate(sweep[:151]):
                value = -1000 + 2000 * step // 150
                self.assertEqual(sample[2], 0, 'pose input must be normalized')
                for axis, actual in enumerate(sample[3]):
                    self.assertAlmostEqual(actual, value / 1000 if axis == index else 0.0)
                display = [0] * 6
                display[index] = value
                expected_lines.append(f'axis={name} x={display[0]} y={display[1]} '
                                      f'z={display[3]} r={display[2]}')
            self.assertEqual(sweep[151][3], (0.0,) * 6)
            expected_lines.append(f'axis={name} complete x=0 y=0 z=0 r=0')
            if number < 3:
                next_start = pose[1 + (number + 1) * 152][0]
                self.assertAlmostEqual(next_start - sweep[151][0], 1.0, delta=0.15)
        walk_after = [sample for sample in samples.values()
                      if sample[1] == 0 and sample[4] == 6 and sample[0] > pose[-1][0]]
        self.assertTrue(walk_after, 'example did not return to Walk')
        self.assertAlmostEqual(walk_after[0][0] - pose[-1][0], 1.0, delta=0.15)
        sit_commands = [sample for sample in samples.values() if sample[4] == 3]
        self.assertTrue(sit_commands, 'example never requested lying down')
        self.assertGreaterEqual(sit_commands[0][0], walk_after[0][0])
        self.assertTrue(all(sample[3] == (0.0,) * 6 for sample in sit_commands))
        self.assertGreaterEqual(lying_feedback_sent - sit_commands[0][0], 3.2)
        self.assertIn('stand-up complete; request pose tracking', output)
        self.assertIn('lying-down complete', output)
        self.assertEqual([line for line in output.splitlines() if line.startswith('axis=')],
                         expected_lines)


if __name__ == '__main__':
    unittest.main()
