import os
import platform
import shutil
import sys
from pathlib import Path

from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext
from setuptools.command.build_py import build_py


ROOT = Path(__file__).resolve().parent


def sdk_arch():
    arch = os.environ.get("BPX_SDK_ARCH", platform.machine()).lower()
    if arch in {"amd64", "x64", "x86_64"}:
        return "x86_64"
    if arch in {"arm64", "aarch64"}:
        return "aarch64"
    return arch


ARCH = sdk_arch()
IS_WINDOWS = sys.platform == "win32"

if IS_WINDOWS and ARCH != "x86_64":
    raise RuntimeError("Windows Python bindings only support x86_64/64-bit Python")

if IS_WINDOWS:
    SDK_RUNTIME_LIBRARY = ROOT / "bin" / f"bpx_sdk_{ARCH}.dll"
    SDK_IMPORT_LIBRARY = ROOT / "lib" / f"bpx_sdk_{ARCH}.lib"
    if not SDK_RUNTIME_LIBRARY.exists():
        raise RuntimeError(f"No BPX SDK runtime DLL found for architecture: {ARCH}")
    if not SDK_IMPORT_LIBRARY.exists():
        raise RuntimeError(f"No BPX SDK import library found for architecture: {ARCH}")
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
    version="1.0.2",
    description="Python bindings for BPX SDK Open",
    packages=["bpx_sdk"],
    package_data={"bpx_sdk": ["lib/*.so", "lib/*.dll", "py.typed", "__init__.pyi"]},
    ext_modules=[
        Extension(
            "bpx_sdk._bpx_sdk",
            sources=["python/bpx_sdk_py.cpp"],
            include_dirs=["include"],
            library_dirs=["lib"],
            libraries=[f"bpx_sdk_{ARCH}"],
            language="c++",
            extra_compile_args=["/std:c++17"] if IS_WINDOWS else ["-std=c++17"],
            runtime_library_dirs=[] if IS_WINDOWS else ["$ORIGIN/lib"],
        )
    ],
    cmdclass={
        "build_py": BuildPyWithSdkLibrary,
        "build_ext": BuildExtWithSdkLibrary,
    },
    python_requires=">=3.8",
)
