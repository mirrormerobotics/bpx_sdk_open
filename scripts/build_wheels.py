#!/usr/bin/env python3
"""Build Python wheel artifacts for the current platform.

Use --cibuildwheel on release builders to build all configured CPython wheels
for the current OS/architecture policy in pyproject.toml.
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PYTHON_ORG_FRAMEWORK = Path("/Library/Frameworks/Python.framework/Versions")
CIBUILDWHEEL_REQUIREMENT = "cibuildwheel<4"


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


def cibuildwheel_env() -> dict[str, str]:
    env = os.environ.copy()
    if (
        sys.platform != "darwin"
        or env.get("CI")
        or env.get("GITHUB_ACTIONS")
        or env.get("CIBW_BUILD")
        or env.get("CIBW_SKIP")
    ):
        return env

    configured_versions = configured_cpython_versions()
    if not configured_versions:
        return env

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
    return env


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
        env = cibuildwheel_env()
        run([sys.executable, "-m", "pip", "install", "--upgrade", CIBUILDWHEEL_REQUIREMENT])
        run(
            [sys.executable, "-m", "cibuildwheel", "--output-dir", out_dir],
            env=env,
        )
    else:
        run([sys.executable, "-m", "pip", "install", "--upgrade", "build"])
        run([sys.executable, "-m", "build", "--wheel", "--outdir", out_dir])

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
