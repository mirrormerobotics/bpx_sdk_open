import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path

from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext
from setuptools.command.build_py import build_py


ROOT = Path(__file__).resolve().parent
DEFAULT_MACOS_DEPLOYMENT_TARGET = "11.0"


def read_sdk_version():
    version_header = ROOT / "include" / "bpx_sdk_version.h"
    pattern = re.compile(r'^\s*#\s*define\s+BPX_SDK_PROJECT_VERSION\s+"([^"]+)"\s*$')

    try:
        lines = version_header.read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        raise RuntimeError(f"Failed to read SDK version from {version_header}") from exc

    for line in lines:
        match = pattern.match(line)
        if match:
            return match.group(1)

    raise RuntimeError(f"BPX_SDK_PROJECT_VERSION not found in {version_header}")


def sdk_arch():
    arch = os.environ.get("BPX_SDK_ARCH", platform.machine()).lower()
    if arch in {"amd64", "x64", "x86_64"}:
        return "x86_64"
    if arch in {"arm64", "aarch64"}:
        return "aarch64"
    return arch


def macos_version_tuple(version):
    parts = []
    for part in version.split("."):
        try:
            parts.append(int(part))
        except ValueError:
            break
    return tuple(parts)


def macos_dylib_minos(dylib):
    try:
        output = subprocess.check_output(["otool", "-l", str(dylib)], text=True)
    except (OSError, subprocess.CalledProcessError):
        return None

    for line in output.splitlines():
        stripped = line.strip()
        if stripped.startswith("minos "):
            return stripped.split()[1]
    return None


ARCH = sdk_arch()
IS_WINDOWS = sys.platform == "win32"
IS_MACOS = sys.platform == "darwin"

if IS_WINDOWS and ARCH != "x86_64":
    raise RuntimeError("Windows Python bindings only support x86_64/64-bit Python")

if IS_MACOS:
    mac_arch = "arm64" if ARCH == "aarch64" else "x86_64"
    macos_deployment_target = os.environ.get(
        "MACOSX_DEPLOYMENT_TARGET", DEFAULT_MACOS_DEPLOYMENT_TARGET
    )
    os.environ["ARCHFLAGS"] = f"-arch {mac_arch}"
    os.environ["MACOSX_DEPLOYMENT_TARGET"] = macos_deployment_target
    os.environ["_PYTHON_HOST_PLATFORM"] = (
        f"macosx-{macos_deployment_target}-{mac_arch}"
    )

if IS_WINDOWS:
    SDK_RUNTIME_LIBRARY = ROOT / "bin" / f"bpx_sdk_{ARCH}.dll"
    SDK_IMPORT_LIBRARY = ROOT / "lib" / f"bpx_sdk_{ARCH}.lib"
    if not SDK_RUNTIME_LIBRARY.exists():
        raise RuntimeError(f"No BPX SDK runtime DLL found for architecture: {ARCH}")
    if not SDK_IMPORT_LIBRARY.exists():
        raise RuntimeError(f"No BPX SDK import library found for architecture: {ARCH}")
elif IS_MACOS:
    SDK_RUNTIME_LIBRARY = ROOT / "lib" / f"libbpx_sdk_{ARCH}.dylib"
    SDK_IMPORT_LIBRARY = SDK_RUNTIME_LIBRARY
    if not SDK_RUNTIME_LIBRARY.exists():
        raise RuntimeError(f"No BPX SDK dynamic library found for architecture: {ARCH}")
    sdk_minos = macos_dylib_minos(SDK_RUNTIME_LIBRARY)
    if sdk_minos and macos_version_tuple(sdk_minos) > macos_version_tuple(
        os.environ["MACOSX_DEPLOYMENT_TARGET"]
    ):
        raise RuntimeError(
            f"{SDK_RUNTIME_LIBRARY} requires macOS {sdk_minos}, which is newer "
            f"than MACOSX_DEPLOYMENT_TARGET={os.environ['MACOSX_DEPLOYMENT_TARGET']}. "
            "Rebuild or replace the SDK dylib with the same or lower deployment target."
        )
else:
    SDK_RUNTIME_LIBRARY = ROOT / "lib" / f"libbpx_sdk_{ARCH}.so"
    SDK_IMPORT_LIBRARY = SDK_RUNTIME_LIBRARY
    if not SDK_RUNTIME_LIBRARY.exists():
        raise RuntimeError(f"No BPX SDK shared library found for architecture: {ARCH}")


def copy_sdk_library(target_package_dir):
    lib_dir = Path(target_package_dir) / "lib"
    lib_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(SDK_RUNTIME_LIBRARY, lib_dir / SDK_RUNTIME_LIBRARY.name)


def remove_python_caches(target_package_dir):
    shutil.rmtree(Path(target_package_dir) / "__pycache__", ignore_errors=True)


def runtime_library_dirs():
    if IS_WINDOWS or IS_MACOS:
        return []
    return ["$ORIGIN/lib"]


def extra_link_args():
    if IS_MACOS:
        return ["-Wl,-rpath,@loader_path/lib"]
    return []


class BuildPyWithSdkLibrary(build_py):
    def run(self):
        super().run()
        package_dir = Path(self.build_lib) / "bpx_sdk"
        copy_sdk_library(package_dir)
        remove_python_caches(package_dir)


class BuildExtWithSdkLibrary(build_ext):
    def run(self):
        super().run()
        for ext in self.extensions:
            ext_path = Path(self.get_ext_fullpath(ext.name))
            copy_sdk_library(ext_path.parent)
            remove_python_caches(ext_path.parent)


setup(
    name="bpx-sdk-open",
    version=read_sdk_version(),
    description="Python bindings for BPX SDK Open",
    packages=["bpx_sdk"],
    package_data={"bpx_sdk": ["lib/*.so", "lib/*.dylib", "lib/*.dll", "py.typed", "__init__.pyi"]},
    ext_modules=[
        Extension(
            "bpx_sdk._bpx_sdk",
            sources=["python/bpx_sdk_py.cpp"],
            include_dirs=["include"],
            library_dirs=["lib"],
            libraries=[f"bpx_sdk_{ARCH}"],
            language="c++",
            extra_compile_args=["/std:c++17"] if IS_WINDOWS else ["-std=c++17"],
            extra_link_args=extra_link_args(),
            runtime_library_dirs=runtime_library_dirs(),
        )
    ],
    cmdclass={
        "build_py": BuildPyWithSdkLibrary,
        "build_ext": BuildExtWithSdkLibrary,
    },
    python_requires=">=3.8",
)
