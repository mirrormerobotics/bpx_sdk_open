"""Loopback integration checks; never connects to robot hardware.

Set BPX_EXAMPLE_BUILD to a CMake build directory to also check C++ examples.
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


class ServerClientExamplesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        robot_features.RobotFeaturesTest.setUpClass()

    @classmethod
    def tearDownClass(cls):
        robot_features.RobotFeaturesTest.tearDownClass()

    def test_interoperability(self):
        servers = [[sys.executable, str(ROOT / 'example/motion_level_control_server_example.py')]]
        clients = [[sys.executable, str(ROOT / 'example/motion_level_control_client_example.py')]]
        if os.environ.get('BPX_EXAMPLE_BUILD'):
            build = Path(os.environ['BPX_EXAMPLE_BUILD'])
            servers.append([str(build / 'motion_level_control_server_example')])
            clients.append([str(build / 'motion_level_control_client_example')])
        for server in servers:
            with self.subTest(server=server[0]):
                with socket.socket() as reservation:
                    reservation.bind(('127.0.0.1', 0))
                    port = reservation.getsockname()[1]
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
                    reservation.bind(('127.0.0.1', 0))
                    state_port = reservation.getsockname()[1]
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp, tempfile.TemporaryFile(mode='w+') as log:
                    udp.bind(('127.0.0.1', 9527))
                    udp.settimeout(2)
                    process = subprocess.Popen(server + ['--robot-ip', '127.0.0.1', '--state-port', str(state_port),
                                               '--listen-port', str(port)], stdout=log, stderr=log)
                    try:
                        deadline = time.monotonic() + 8
                        while True:
                            try:
                                with socket.create_connection(('127.0.0.1', port), timeout=.2) as conn:
                                    self.assertIn(b'OK', conn.recv(1024))
                                break
                            except OSError:
                                self.assertIsNone(process.poll())
                                self.assertLess(time.monotonic(), deadline)
                                time.sleep(.05)
                        def command(client, *words):
                            return subprocess.run(client + ['--server-port', str(port), *words],
                                                  capture_output=True, text=True, timeout=5)
                        for client in clients:
                            help_result = subprocess.run(client + ['--help'], capture_output=True,
                                                         text=True, timeout=5)
                            self.assertEqual(help_result.returncode, 0, help_result.stderr)
                            self.assertIn('charging-sit', help_result.stdout)
                            self.assertIn('charging-sit', command(client, 'help').stdout)
                            deadline = time.monotonic() + 5
                            while True:
                                result = command(client, 'status')
                                self.assertEqual(result.returncode, 0, result.stderr)
                                if 'connected=1' in result.stdout:
                                    break
                                self.assertLess(time.monotonic(), deadline)
                                time.sleep(.05)
                            for text in ('SN=original-SN', 'model=BPX', 'control_mode=RemoteControl (1)',
                                         'battery_level=', 'battery_current=', 'charger_in1=', 'charger_in2='):
                                self.assertIn(text, result.stdout)
                            for words in [('jump', 'invalid'), ('velocity', 'nan', '0', '0')]:
                                result = command(client, *words)
                                self.assertNotEqual(result.returncode, 0)
                                self.assertIn('ERR', result.stdout)
                            for direction, subgait in [('up',0),('front',1),('back',2),('left',-1),('right',-2)]:
                                result = command(client, 'jump', direction)
                                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                                deadline = time.monotonic() + 3
                                while True:
                                    packet = udp.recv(1024)
                                    if len(packet) == 56 and packet[5] == 12 and struct.unpack_from('b', packet, 34)[0] == subgait:
                                        break
                                    self.assertLess(time.monotonic(), deadline)
                            result = command(client, 'charging-sit')
                            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                            self.assertIn('OK charging sit-down command sent', result.stdout)
                            deadline = time.monotonic() + 3
                            while True:
                                self.assertLess(time.monotonic(), deadline)
                                packet = udp.recv(1024)
                                if len(packet) == 56 and packet[4] == 24:
                                    break
                            self.assertEqual(packet[5], 0)
                            self.assertEqual(struct.unpack_from('<6f', packet, 8), (0.0,) * 6)
                            for current, previous, expected in [
                                    (24, 6, ('motion_state=24(ChargingSitDown)', 'last_motion_state=6(Motion)')),
                                    (2, 24, ('motion_state=2(Passive)', 'last_motion_state=24(ChargingSitDown)'))]:
                                payload = bytearray(20)
                                payload[0], payload[2] = current, previous
                                deadline = time.monotonic() + 3
                                while True:
                                    udp.sendto(struct.pack('<HHI', 0x10, 20, current) + payload,
                                               ('127.0.0.1', state_port))
                                    result = command(client, 'status')
                                    self.assertEqual(result.returncode, 0, result.stderr)
                                    if all(' ' + field in result.stdout for field in expected):
                                        break
                                    self.assertLess(time.monotonic(), deadline, result.stdout)
                                    time.sleep(.02)
                        self.assertEqual(command(clients[-1], 'quit').returncode, 0)
                        self.assertEqual(process.wait(timeout=5), 0)
                        log.seek(0)
                        self.assertRegex(log.read(), r'robot state: \d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3} .*battery_level=.*battery_current=.*charger_in1=.*charger_in2=')
                    finally:
                        if process.poll() is None:
                            process.terminate()
                            process.wait(timeout=5)
