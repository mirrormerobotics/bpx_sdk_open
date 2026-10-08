"""Exercise Python bindings against a loopback peer; no robot hardware is used."""
import multiprocessing
import select
import socket
import struct
import time
import unittest

import bpx_sdk


def peer(ready):
    clients = {}
    identity = (0, "original-SN")
    control_mode = 0
    server = socket.socket()
    try:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(('127.0.0.1', 10860))
        server.listen()
        ready.send(None)
        while True:
            readable, _, _ = select.select([ready, server, *clients], [], [], 0.02)
            for sock in readable:
                if sock is ready:
                    identity, control_mode = ready.recv()
                    ready.send(None)
                    continue
                if sock is server:
                    client, _ = server.accept()
                    clients[client] = [b'', None]
                    continue
                data = sock.recv(1024)
                if not data:
                    sock.close()
                    del clients[sock]
                    continue
                clients[sock][0] += data
                while len(clients[sock][0]) >= 24:
                    req, clients[sock][0] = clients[sock][0][:24], clients[sock][0][24:]
                    session = struct.unpack_from('<H', req)[0]
                    mode = req[10]
                    if mode == 4:
                        response = bytearray(64)
                        struct.pack_into('<BBH', response, 0, 4, 1, session)
                        sock.sendall(response)
                    elif mode == 6:
                        wire_model, sn = identity
                        sock.sendall(struct.pack('<BBHBBBB32s56s',
                            6, 1, session, 1, wire_model, len(sn), 0, sn.encode(), b''))
                    elif mode == 5:
                        response = bytearray(32)
                        struct.pack_into('<BBH', response, 0, 5, 0, session)
                        response[28] = 2
                        sock.sendall(response)
                    else:
                        response = bytearray(32)
                        struct.pack_into('<HBxHBB', response, 0, session, 1, 50, mode, 3)
                        clients[sock][1] = response
            for sock, (_, response) in list(clients.items()):
                if response is not None:
                    try:
                        struct.pack_into('<III', response, 20,
                                         0x434D0001 if control_mode is not None else 0,
                                         control_mode if control_mode is not None else 0, 0)
                        sock.sendall(response)
                    except OSError:
                        sock.close()
                        del clients[sock]
    except Exception as error:
        ready.send(repr(error))
        raise
    finally:
        server.close()


class RobotFeaturesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        context = multiprocessing.get_context('spawn')
        read, write = context.Pipe()
        cls.peer = context.Process(target=peer, args=(write,), daemon=True)
        cls.peer.start()
        if not read.poll(5):
            cls.peer.terminate()
            raise RuntimeError('loopback peer did not start')
        error = read.recv()
        cls.channel = read
        write.close()
        if error:
            cls.peer.join(1)
            raise RuntimeError(error)

    @classmethod
    def tearDownClass(cls):
        cls.peer.terminate()
        cls.peer.join(5)
        cls.channel.close()

    def wait(self, predicate):
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            if predicate():
                return
            time.sleep(0.01)
        self.assertTrue(predicate())

    def connect(self, kind):
        sdk = kind()
        self.addCleanup(sdk.disconnect)
        sdk.setRobotIp('127.0.0.1')
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        sdk.setRobotStateUploadPort(port)
        if isinstance(sdk, bpx_sdk.JointLevelControl):
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
                reservation.bind(('127.0.0.1', 0))
                sdk.setJointStateUploadPort(reservation.getsockname()[1])
        sdk.setRobotStateUploadRate(50)
        self.assertTrue(sdk.connect())
        self.assertIsNotNone(sdk.getRobotSerialNumber())
        self.assertIsNotNone(sdk.getRobotModel())
        self.wait(sdk.isConnected)
        return sdk, port

    def configure_peer(self, wire_model=0, sn='original-SN', control_mode=0):
        self.channel.send(((wire_model, sn), control_mode))
        self.assertTrue(self.channel.poll(3), 'peer configuration timed out')
        self.assertIsNone(self.channel.recv())

    def test_identity_on_all_three_classes(self):
        for kind in (bpx_sdk.RequestRobotState, bpx_sdk.MotionLevelControl, bpx_sdk.JointLevelControl):
            for wire, model, sn in [(0, bpx_sdk.RobotModel.BPX, 'original-SN'),
                                    (1, bpx_sdk.RobotModel.BPXPro, 'S' * 32),
                                    (2, bpx_sdk.RobotModel.Unknown, ''),
                                    (3, bpx_sdk.RobotModel.BPW, 'BPW-SN'),
                                    (255, bpx_sdk.RobotModel.Unknown, 'future-SN')]:
                with self.subTest(kind=kind.__name__, model=model, sn=sn):
                    self.configure_peer(wire, sn)
                    sdk, port = self.connect(kind)
                    self.assertEqual(sdk.getRobotSerialNumber(), sn)
                    self.assertEqual(bpx_sdk.RobotModel(sdk.getRobotModel()), model)
                    sdk.disconnect()
                    self.assertIsNone(sdk.getRobotSerialNumber())
                    self.assertIsNone(sdk.getRobotModel())

    def test_control_mode_on_all_three_classes(self):
        for kind in (bpx_sdk.RequestRobotState, bpx_sdk.MotionLevelControl, bpx_sdk.JointLevelControl):
            with self.subTest(kind=kind.__name__):
                self.configure_peer()
                sdk, _ = self.connect(kind)
                for wire, expected in [(0, bpx_sdk.ControlMode.RemoteControl),
                                       (1, bpx_sdk.ControlMode.Navigator),
                                       (255, bpx_sdk.ControlMode.Unknown), (None, None)]:
                    self.configure_peer(control_mode=wire)
                    self.wait(lambda: sdk.getControlMode() == expected)
                    if expected is not None:
                        self.assertEqual(bpx_sdk.ControlMode(sdk.getControlMode()), expected)
                sdk.disconnect()
                self.assertIsNone(sdk.getControlMode())

    def test_charger_feedback_on_all_three_classes(self):
        for kind in (bpx_sdk.RequestRobotState, bpx_sdk.MotionLevelControl, bpx_sdk.JointLevelControl):
            with self.subTest(kind=kind.__name__):
                self.configure_peer()
                sdk, port = self.connect(kind)
                self.assertIsNone(sdk.getChargerIn1())
                self.assertIsNone(sdk.getChargerIn2())
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
                    for flags in (0, 1, 2, 3, 0xfc):
                        payload = bytearray(32)
                        payload[1] = flags
                        sender.sendto(struct.pack('<HHI', 1, 32, flags + 1) + payload,
                                      ('127.0.0.1', port))
                        self.wait(lambda: (sdk.getChargerIn1(), sdk.getChargerIn2()) ==
                                  (flags & 1, (flags >> 1) & 1))
                sdk.disconnect()

    def test_charging_sit_down_command_and_feedback(self):
        motion = bpx_sdk.MotionLevelControl()
        self.assertIs(motion.setChargingSitDown(), False)
        self.configure_peer()
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
            receiver.bind(('127.0.0.1', 9527))
            receiver.settimeout(2)
            sdk, port = self.connect(bpx_sdk.MotionLevelControl)
            self.wait(lambda: sdk.setVelocity(0.3, 0.1, 0.2))
            self.assertIs(sdk.setChargingSitDown(), True)
            deadline = time.monotonic() + 3
            while True:
                self.assertLess(time.monotonic(), deadline)
                packet = receiver.recv(1024)
                if len(packet) == 56 and packet[4] == 24:
                    break
            self.assertEqual(packet[5], 0)  # Walk gait
            self.assertEqual(struct.unpack_from('<6f', packet, 8), (0.0,) * 6)
            self.assertIsNone(sdk.getCurrentMotionState())
            self.assertIsNone(sdk.getLastMotionState())
            for timestamp, current, previous in [(1, 24, 6), (2, 2, 24)]:
                payload = bytearray(20)
                payload[0], payload[2] = current, previous
                receiver.sendto(struct.pack('<HHI', 0x10, 20, timestamp) + payload,
                                ('127.0.0.1', port))
                self.wait(lambda: sdk.getMotionStateTimestamp() == timestamp)
                self.assertEqual(sdk.getCurrentMotionState(), current)
                self.assertEqual(sdk.getLastMotionState(), previous)
                self.assertEqual(bpx_sdk.MotionState(24).name, 'ChargingSitDown')
            sdk.disconnect()
            self.assertIs(sdk.setChargingSitDown(), False)

    def test_pose_tracking_and_four_axis_input(self):
        axes = (0.2, -0.3, 1.0, -1.0)
        expected_axes = (0.2, -0.3, -1.0, 1.0, 0.0, 0.0)
        disconnected = bpx_sdk.MotionLevelControl()
        self.assertIs(disconnected.setPoseTracking(), False)
        self.assertIs(disconnected.setPose(*axes), False)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
            receiver.bind(('127.0.0.1', 9527))
            receiver.settimeout(2)
            sdk, _ = self.connect(bpx_sdk.MotionLevelControl)
            sdk.setVelocityControlFlag(True)
            self.wait(lambda: sdk.setVelocity(0.3, 0.1, 0.2))
            self.assertIs(sdk.setPose(*axes), False)

            def receive_matching(predicate):
                deadline = time.monotonic() + 3
                while time.monotonic() < deadline:
                    packet = receiver.recv(1024)
                    self.assertEqual(len(packet), 56)
                    if predicate(packet):
                        return packet
                self.fail('expected pose packet did not arrive')

            self.assertIs(sdk.setPoseTracking(), True)
            packet = receive_matching(lambda p: p[5] == 7)
            self.assertEqual((packet[4], packet[32], packet[34]), (6, 0, 0))
            self.assertEqual(struct.unpack_from('<6f', packet, 8), (0.0,) * 6)
            selected_version = struct.unpack_from('<I', packet, 40)[0]
            self.assertIs(sdk.setPose(*axes), True)
            packet = receive_matching(lambda p: p[5] == 7 and
                struct.unpack_from('<I', p, 40)[0] != selected_version)
            self.assertEqual(packet[32], 0)
            for actual, expected in zip(struct.unpack_from('<6f', packet, 8), expected_axes):
                self.assertAlmostEqual(actual, expected)
            version = struct.unpack_from('<I', packet, 40)[0]
            for index in range(4):
                for invalid in (1.1, -1.1, float('nan'), float('inf'), -float('inf')):
                    bad_axes = list(axes)
                    bad_axes[index] = invalid
                    self.assertIs(sdk.setPose(*bad_axes), False)
            packet = receive_matching(lambda p: p[5] == 7)
            self.assertEqual(struct.unpack_from('<I', packet, 40)[0], version)
            for invalid in ((), (0, 0, 0), (0,) * 5, (0,) * 6, ([0] * 6,),
                            (None, 0, 0, 0), (0, 'bad', 0, 0)):
                with self.assertRaises(TypeError):
                    sdk.setPose(*invalid)
            sdk.setWalk()
            packet = receive_matching(lambda p: p[5] == 0)
            self.assertEqual(packet[32], 1)
            self.assertEqual(struct.unpack_from('<6f', packet, 8), (0.0,) * 6)
            self.assertIs(sdk.setPose(*axes), False)
            sdk.disconnect()
            self.assertIs(sdk.setPoseTracking(), False)

    def test_jump_commands_and_repeated_requests(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
            receiver.bind(('127.0.0.1', 9527))
            receiver.settimeout(2)
            sdk, _ = self.connect(bpx_sdk.MotionLevelControl)
            self.wait(lambda: sdk.setVelocity(0, 0, 0))
            previous_version = None
            for name, sub_gait in [('setUpJump', 0), ('setFrontJump', 1), ('setBackJump', 2),
                                    ('setLeftJump', -1), ('setRightJump', -2)]:
                for _ in range(2):
                    self.assertIsNone(getattr(sdk, name)())
                    deadline = time.monotonic() + 3
                    while True:
                        self.assertLess(time.monotonic(), deadline)
                        packet = receiver.recv(1024)
                        self.assertEqual(len(packet), 56)
                        version = struct.unpack_from('<I', packet, 40)[0]
                        if (packet[5] == 12 and struct.unpack_from('b', packet, 34)[0] == sub_gait
                                and version != previous_version):
                            break
                    previous_version = version
                    self.assertEqual(struct.unpack_from('<6f', packet, 8), (0.0,) * 6)
            sdk.disconnect()

    def test_enum_values_and_argument_validation(self):
        self.assertEqual([int(x) for x in bpx_sdk.RobotModel], [0, 1, 2, 3])
        self.assertEqual([int(x) for x in bpx_sdk.ControlMode], [0, 1, 2])
        self.assertEqual(bpx_sdk.MotionGait.Jump, 12)
        self.assertEqual(bpx_sdk.MotionState.ChargingSitDown, 24)
        sdk = bpx_sdk.MotionLevelControl()
        for name in ('getRobotSerialNumber', 'getRobotModel', 'getControlMode', 'getChargerIn1', 'getChargerIn2', 'setUpJump', 'setFrontJump',
                     'setBackJump', 'setLeftJump', 'setRightJump', 'setChargingSitDown'):
            with self.assertRaises(TypeError):
                getattr(sdk, name)(1)


if __name__ == '__main__':
    unittest.main()
