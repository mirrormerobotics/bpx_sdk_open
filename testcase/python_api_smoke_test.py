"""Verify the installed wheel exposes the public Python robot APIs."""
import bpx_sdk

assert [(model.name, model.value) for model in bpx_sdk.RobotModel] == [
    ("Unknown", 0), ("BPX", 1), ("BPXPro", 2), ("BPW", 3)]
assert [(mode.name, mode.value) for mode in bpx_sdk.ControlMode] == [
    ("Unknown", 0), ("RemoteControl", 1), ("Navigator", 2)]
assert bpx_sdk.MotionGait.Jump == 12
assert bpx_sdk.MotionState.ChargingSitDown == 24
for kind in (bpx_sdk.RequestRobotState, bpx_sdk.MotionLevelControl, bpx_sdk.JointLevelControl):
    robot = kind()
    assert robot.getRobotSerialNumber() is None
    assert robot.getRobotModel() is None
    assert robot.getControlMode() is None
    assert robot.getChargerIn1() is None
    assert robot.getChargerIn2() is None
motion = bpx_sdk.MotionLevelControl()
assert not hasattr(motion, "setUpright")
assert bpx_sdk.MotionGait.PoseTracking == 7
assert bpx_sdk.MotionGait.Tumbler == 11
assert motion.setTumbler() is False
assert motion.setPoseTracking() is False
assert motion.setPose(0.0, 0.0, 0.0, 0.0) is False
assert motion.setChargingSitDown() is False
for name in ("setUpJump", "setFrontJump", "setBackJump", "setLeftJump", "setRightJump"):
    assert callable(getattr(motion, name))
print("Python SDK API smoke test passed")
