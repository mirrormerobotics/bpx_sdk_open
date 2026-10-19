#!/usr/bin/env python3
"""Build Python wheel artifacts for the current platform.

Use --cibuildwheel on release builders to build all configured CPython wheels
for the current OS/architecture policy in pyproject.toml. CPython 3.8 wheels
are built in a second pass with a legacy cibuildwheel 3.x install, because
cibuildwheel 4.x dropped Python 3.8 support; see scripts/cibuildwheel-cp38.toml.
"""

from __future__ import annotations

import argparse
import fnmatch
import os
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PYTHON_ORG_FRAMEWORK = Path("/Library/Frameworks/Python.framework/Versions")
CIBUILDWHEEL_REQUIREMENT = "cibuildwheel>=4.3.1,<5"
LEGACY_CIBUILDWHEEL_REQUIREMENT = "cibuildwheel==3.4.1"
LEGACY_CIBW_CONFIG = ROOT / "scripts" / "cibuildwheel-cp38.toml"
LEGACY_VENV_DIR = ROOT / ".venv-cibw38"
MODERN_CP_TAGS = [f"cp3{minor}-manylinux_x86_64" for minor in range(9, 16)]
LEGACY_CP38_TAG = ["cp38-manylinux_x86_64"]


def run(args: list[str], env: dict[str, str] | None = None) -> None:
    print("+", " ".join(args), flush=True)
    subprocess.check_call(args, cwd=ROOT, env=env)


def configured_cpython_versions() -> list[str]:
    pyproject = ROOT / "pyproject.toml"
    text = pyproject.read_text(encoding="utf-8")
    match = re.search(r"(?m)^build\s*=\s*\"([^\"]+)\"", text)
    if not match:
        return []

    versions: list[str] = []
    for token in match.group(1).split():
        version_match = re.match(r"cp(\d)(\d{1,2})-", token)
        if not version_match:
            continue
        major, minor = version_match.groups()
        version = f"{major}.{minor}"
        if version not in versions:
            versions.append(version)
    return versions


def python_tag(version: str) -> str:
    major, minor = version.split(".", 1)
    return f"cp{major}{minor}"


def env_selects_any(env: dict[str, str], tags: list[str]) -> bool:
    build = env.get("CIBW_BUILD")
    if not build:
        return True
    return any(
        fnmatch.fnmatchcase(tag, token) for token in build.split() for tag in tags
    )


def installed_python_org_versions() -> set[str]:
    if not PYTHON_ORG_FRAMEWORK.exists():
        return set()

    versions: set[str] = set()
    for python in sorted(PYTHON_ORG_FRAMEWORK.glob("*/bin/python3")):
        try:
            output = subprocess.check_output(
                [
                    str(python),
                    "-c",
                    "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')",
                ],
                text=True,
            )
        except (OSError, subprocess.CalledProcessError):
            continue
        versions.add(output.strip())
    return versions


def cibuildwheel_envs() -> tuple[dict[str, str], dict[str, str] | None]:
    """Return the modern-pass environment and the legacy cp38-pass environment.

    The legacy environment is None when the cp38 pass should be skipped
    (local macOS builds without an installed python.org CPython 3.8).
    """
    env = os.environ.copy()
    legacy_env = env.copy()
    if (
        sys.platform != "darwin"
        or env.get("CI")
        or env.get("GITHUB_ACTIONS")
        or env.get("CIBW_BUILD")
        or env.get("CIBW_SKIP")
    ):
        return env, legacy_env

    configured_versions = configured_cpython_versions()
    if not configured_versions:
        return env, legacy_env

    installed_versions = installed_python_org_versions()
    build_versions = [
        version for version in configured_versions if version in installed_versions
    ]

    if not build_versions:
        configured = ", ".join(configured_versions)
        installed = ", ".join(sorted(installed_versions)) or "none"
        raise SystemExit(
            "No configured python.org CPython interpreters were found for "
            "local macOS cibuildwheel builds. "
            f"Configured versions: {configured}. "
            f"Installed python.org versions: {installed}. "
            "Install one of the configured python.org packages, add an "
            "installed version to pyproject.toml, set CIBW_BUILD/CIBW_SKIP "
            "explicitly, or build only the current interpreter with "
            "`python3 scripts/build_wheels.py --out-dir wheelhouse`."
        )

    env["CIBW_BUILD"] = " ".join(f"{python_tag(version)}-*" for version in build_versions)
    env.setdefault("CIBW_CACHE_PATH", str(ROOT / ".cibuildwheel-cache"))
    if len(build_versions) != len(configured_versions):
        missing_versions = [
            version for version in configured_versions if version not in installed_versions
        ]
        print(
            "Local macOS cibuildwheel: skipping missing python.org CPython "
            f"interpreters: {', '.join(missing_versions)}",
            flush=True,
        )
    if "3.8" in installed_versions:
        legacy_env["CIBW_BUILD"] = "cp38-*"
        legacy_env.setdefault("CIBW_CACHE_PATH", str(ROOT / ".cibuildwheel-cache"))
    else:
        print(
            "Local macOS cibuildwheel: skipping the legacy CPython 3.8 pass; "
            "python.org CPython 3.8 is not installed.",
            flush=True,
        )
        legacy_env = None
    return env, legacy_env


def legacy_cibuildwheel_python() -> str:
    if os.name == "nt":
        python = LEGACY_VENV_DIR / "Scripts" / "python.exe"
    else:
        python = LEGACY_VENV_DIR / "bin" / "python"
    if not python.exists():
        run([sys.executable, "-m", "venv", str(LEGACY_VENV_DIR)])
    run([str(python), "-m", "pip", "install", LEGACY_CIBUILDWHEEL_REQUIREMENT])
    return str(python)


def main() -> int:
    parser = argparse.ArgumentParser(description="Build BPX SDK Python wheels.")
    parser.add_argument(
        "--out-dir",
        default="wheelhouse",
        help="Directory for generated wheel files. Defaults to wheelhouse.",
    )
    parser.add_argument(
        "--cibuildwheel",
        action="store_true",
        help="Use cibuildwheel to build every configured CPython wheel for this OS.",
    )
    args = parser.parse_args()

    out_dir = str(Path(args.out_dir))

    if args.cibuildwheel:
        env, legacy_env = cibuildwheel_envs()
        if env_selects_any(env, MODERN_CP_TAGS):
            run([sys.executable, "-m", "pip", "install", "--upgrade", CIBUILDWHEEL_REQUIREMENT])
            run(
                [sys.executable, "-m", "cibuildwheel", "--allow-empty", "--output-dir", out_dir],
                env=env,
            )
        else:
            print(
                "CIBW_BUILD selects no modern CPython; skipping the main "
                "cibuildwheel pass.",
                flush=True,
            )
        if legacy_env is not None and env_selects_any(legacy_env, LEGACY_CP38_TAG):
            legacy_python = legacy_cibuildwheel_python()
            run(
                [
                    legacy_python,
                    "-m",
                    "cibuildwheel",
                    "--allow-empty",
                    "--config-file",
                    str(LEGACY_CIBW_CONFIG),
                    "--output-dir",
                    out_dir,
                ],
                env=legacy_env,
            )
        elif legacy_env is not None:
            print(
                "CIBW_BUILD selects no CPython 3.8; skipping the legacy "
                "cibuildwheel pass.",
                flush=True,
            )
    else:
        run([sys.executable, "-m", "pip", "install", "--upgrade", "build"])
        run([sys.executable, "-m", "build", "--wheel", "--outdir", out_dir])

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
