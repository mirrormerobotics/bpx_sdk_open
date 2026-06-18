import os
import platform
import sys
from pathlib import Path
from enum import IntEnum

_package_dir = Path(__file__).resolve().parent
_dll_directory_handle = None


def _has_native_extension(package_dir):
    return any(package_dir.glob("_bpx_sdk*.pyd")) or any(package_dir.glob("_bpx_sdk*.so"))


def _find_installed_native_package_dir():
    for entry in sys.path:
        if not entry:
            continue
        try:
            candidate = (Path(entry).resolve() / "bpx_sdk").resolve()
        except OSError:
            continue
        if candidate == _package_dir:
            continue
        if _has_native_extension(candidate):
            return candidate
    return None


_native_package_dir = _package_dir
if not _has_native_extension(_native_package_dir):
    _installed_package_dir = _find_installed_native_package_dir()
    if _installed_package_dir is not None:
        _native_package_dir = _installed_package_dir
        __path__.append(str(_installed_package_dir))

if sys.platform == "win32":
    _machine = platform.machine().lower()
    if _machine not in {"amd64", "x64", "x86_64"}:
        raise ImportError("bpx_sdk only supports 64-bit Python on Windows")
    _dll_dir = _native_package_dir / "lib"
    if _dll_dir.exists():
        _dll_directory_handle = os.add_dll_directory(str(_dll_dir))

from ._bpx_sdk import (
    DEFAULT_CLIENT_JOINT_STATE_UDP_PORT,
    DEFAULT_CLIENT_ROBOT_STATE_UDP_PORT,
    DEFAULT_SERVER_IP,
    JOINT_COUNT,
    JointLevelControl,
    MotionLevelControl,
    RequestRobotState,
    __version__,
)
from . import _bpx_sdk as _native


class MotionState(IntEnum):
    LyingDown = _native.MOTION_STATE_LYING_DOWN
    StandingUp = _native.MOTION_STATE_STANDING_UP
    Passive = _native.MOTION_STATE_PASSIVE
    SitDown = _native.MOTION_STATE_SIT_DOWN
    Motion = _native.MOTION_STATE_MOTION


class MotionGait(IntEnum):
    Walk = _native.MOTION_GAIT_WALK
    Bipedal = _native.MOTION_GAIT_BIPEDAL
    Flip = _native.MOTION_GAIT_FLIP
    WalkPhase = _native.MOTION_GAIT_WALK_PHASE
    PoseTracking = _native.MOTION_GAIT_POSE_TRACKING
    Running = _native.MOTION_GAIT_RUNNING


__all__ = [
    "__version__",
    "DEFAULT_CLIENT_JOINT_STATE_UDP_PORT",
    "DEFAULT_CLIENT_ROBOT_STATE_UDP_PORT",
    "DEFAULT_SERVER_IP",
    "JOINT_COUNT",
    "JointLevelControl",
    "MotionGait",
    "MotionLevelControl",
    "MotionState",
    "RequestRobotState",
]
