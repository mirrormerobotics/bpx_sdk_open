#!/usr/bin/env python3
"""通过 GitHub Actions 构建三端 Python wheels，下载到指定目录，可选显示用量。"""

import argparse
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time
import unicodedata
import zipfile


REPO_ROOT = Path(__file__).resolve().parent.parent
WORKFLOW = "build-wheels.yml"
ARTIFACTS = {
    "bpx-sdk-open-wheels-ubuntu-22.04": ("x86_64",),
    "bpx-sdk-open-wheels-ubuntu-22.04-arm": ("aarch64",),
    "bpx-sdk-open-wheels-windows-2022": ("win_amd64",),
    "bpx-sdk-open-wheels-macos-14": ("arm64",),
}
# Standard published allowances, not an account-specific entitlement response.
# https://docs.github.com/en/billing/concepts/product-billing/github-actions
PLAN_MINUTES = {"free": 2000, "pro": 3000, "team": 3000}
# Historical assumptions only; these multipliers are absent from current docs.
# actions_linux_arm is counted like Linux x64 (conservative; per-minute pricing
# suggests it is slightly cheaper than x64).
HISTORICAL_MULTIPLIERS = {"actions_linux": 1, "actions_linux_arm": 1,
                          "actions_windows": 2, "actions_macos": 10}
STATUS_LABELS = {
    "queued": "排队中", "in_progress": "运行中", "completed": "已完成",
    "requested": "已请求", "waiting": "等待中", "pending": "待处理",
    "success": "成功", "failure": "失败", "cancelled": "已取消",
    "timed_out": "已超时", "action_required": "需要人工处理",
    "neutral": "中性结果", "skipped": "已跳过", "stale": "已过期",
    "startup_failure": "启动失败",
}
UNIT_LABELS = {
    "minutes": "分钟", "minute": "分钟", "seconds": "秒",
    "gigabyte-hours": "GB·小时", "gb-hours": "GB·小时",
    "gigabyte-months": "GB·月", "gb-months": "GB·月",
    "gigabytes": "GB",
}


class ChineseArgumentParser(argparse.ArgumentParser):
    """Localize argparse text without depending on the system locale."""

    def format_help(self):
        return (super().format_help().replace("usage: ", "用法：")
                .replace("optional arguments:", "选项：")
                .replace("options:", "选项：").replace("positional arguments:", "位置参数："))

    def format_usage(self):
        return super().format_usage().replace("usage: ", "用法：")

    def error(self, message):
        for original, translated in (
            ("not allowed with argument", "不能同时使用参数"),
            ("expected one argument", "需要提供一个值"),
            ("unrecognized arguments:", "无法识别的参数："),
            ("argument ", "参数 "),
        ):
            message = message.replace(original, translated)
        self.print_usage(sys.stderr)
        self.exit(2, "{}：参数错误：{}\n".format(self.prog, message))


def show_quota_estimate(items, plan):
    allowance = PLAN_MINUTES.get(plan)
    if allowance is None:
        print("无法估算剩余分钟：套餐未知或暂不支持。", flush=True)
        return
    minutes = [item for item in items if item["product"].lower() == "actions"
               and item["unitType"].lower() == "minutes"]
    if not minutes:
        print("无法估算剩余分钟：未查询到构建分钟记录。", flush=True)
        return
    used = 0.0
    unknown = []
    for item in minutes:
        multiplier = HISTORICAL_MULTIPLIERS.get(item["sku"])
        quantity = float(item["grossQuantity"])
        if multiplier is None or not math.isfinite(quantity) or quantity < 0:
            unknown.append(item["sku"])
            continue
        used += quantity * multiplier
    if unknown:
        print("警告：以下 runner SKU 的倍率未知，未计入估算（已用量可能被低估）：{}。".format(
            "、".join(sorted(set(unknown)))), flush=True)
    remaining = max(0, allowance - used)
    print("仅供估算：已用约 {:g} / {:g} 分钟；剩余约 {:g} 分钟（{:.1f}%）。".format(
        used, allowance, remaining, remaining / allowance * 100), flush=True)
    print("估算依据：历史倍率 Linux ×1、Linux ARM64 ×1、Windows ×2、macOS ×10；"
          "将所有已报告的构建分钟计入额度。"
          "当前规则、公共仓库用量和数据延迟可能影响结果。此数值不是 GitHub 确认的余额。", flush=True)


def command(args, timeout=60):
    try:
        result = subprocess.run(
            args, cwd=REPO_ROOT, text=True, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, timeout=timeout,
        )
    except subprocess.TimeoutExpired as error:
        raise RuntimeError("命令执行超时（{} 秒）：{}".format(timeout, " ".join(args))) from error
    if result.returncode:
        detail = result.stderr.strip() or result.stdout.strip() or " ".join(args)
        raise RuntimeError("命令执行失败（退出码 {}），原始信息：\n{}".format(result.returncode, detail))
    return result.stdout.strip()


def api(endpoint, *args):
    output = command([
        "gh", "api", "--hostname", "github.com",
        "-H", "X-GitHub-Api-Version: 2026-03-10", endpoint, *args,
    ])
    if not output:
        raise RuntimeError("GitHub API 返回了空响应。")
    return json.loads(output)


def show_usage(repo):
    """Display owner-wide billing data; unavailable billing must not block builds."""
    owner = repo.split("/", 1)[0]
    now = datetime.now(timezone.utc)
    print("GitHub Actions 用量：{}（UTC 自然月 {:04d}-{:02d}）".format(
        owner, now.year, now.month), flush=True)
    billing_url = "https://github.com/settings/billing/usage"
    try:
        metadata = api("repos/" + repo)
        organization = metadata["owner"]["type"] == "Organization"
        if organization:
            billing_url = "https://github.com/organizations/{}/settings/billing/usage".format(owner)
        visibility = {"private": "私有", "public": "公开", "internal": "内部"}.get(
            metadata["visibility"], metadata["visibility"])
        print("仓库：{}（{}）".format(repo, visibility), flush=True)
        if metadata["visibility"] == "public":
            print("此公共仓库使用标准 GitHub 托管 runner 免费。", flush=True)
        plan = None
        try:
            account = api(("orgs/" if organization else "users/") + owner)
            plan = (account.get("plan") or {}).get("name")
            allowance = PLAN_MINUTES.get(plan)
            print("账户套餐：{}；每月标准额度：{}。".format(
                plan or "未知", "{} 分钟（套餐标准参考值，非账户实时权益）".format(allowance)
                if allowance is not None else "未知"), flush=True)
        except (RuntimeError, OSError, ValueError, KeyError, TypeError, subprocess.TimeoutExpired) as error:
            print("无法查询账户套餐：{}。将继续查询用量。".format(error), flush=True)
        endpoint = "{}/{}/settings/billing/usage/summary".format(
            "organizations" if organization else "users", owner)
        report = api(endpoint, "--method", "GET", "-f", "product=Actions",
                     "-F", "year=" + str(now.year), "-F", "month=" + str(now.month))
        items = report["usageItems"]
        if not isinstance(items, list):
            raise ValueError("账单响应格式异常：usageItems 不是列表")
        lines = []
        total = 0.0
        for item in items:
            if item["product"].lower() != "actions":
                continue
            net = float(item["netAmount"])
            total += net
            lines.append("  {}：{:g} {}；折扣前 ${:.4f}，折扣 ${:.4f}，净额 ${:.4f}".format(
                item["sku"], float(item["grossQuantity"]),
                UNIT_LABELS.get(item["unitType"].lower(), item["unitType"]),
                float(item["grossAmount"]), float(item["discountAmount"]), net))
        print("统计范围：计入 {} 账单的所有仓库，包含此 SDK 以外的用量。".format(owner), flush=True)
        if lines:
            print("\n".join(lines), flush=True)
            print("已报告的 Actions 净费用：${:.4f} 美元".format(total), flush=True)
        else:
            print("本月未返回 Actions 用量记录，不能据此认定额度尚未使用。", flush=True)
        show_quota_estimate(items, plan)
        print("此 API 不提供准确剩余额度和预算。存储计量为累计用量，不代表当前剩余空间。", flush=True)
        success = True
    except (RuntimeError, OSError, ValueError, KeyError, TypeError, subprocess.TimeoutExpired) as error:
        print("无法查询 Actions 用量：{}".format(error), flush=True)
        print("用量及剩余额度未知，不能视为零。查询组织账单需要管理员身份及相应 token 权限；"
              "企业统一计费可能需要在企业层查看。", flush=True)
        success = False
    print("账单详情：" + billing_url, flush=True)
    return success


def dispatch(repo, ref):
    result = api(
        "repos/{}/actions/workflows/{}/dispatches".format(repo, WORKFLOW),
        "--method", "POST", "--raw-field", "ref=" + ref,
    )
    run_id = result.get("workflow_run_id")
    if not isinstance(run_id, int) or run_id <= 0:
        raise RuntimeError("GitHub 未返回运行 ID；重试前请先检查 Actions 页面。")
    return run_id


STEP_LABELS = {
    "Set up job": "准备运行环境",
    "Check out repository": "检出代码",
    "Set up QEMU for Linux aarch64 wheels": "准备 Linux aarch64 模拟环境",
    "Set up Python": "准备 Python",
    "Build wheels": "编译和测试 wheels",
    "Upload wheel artifacts": "上传 wheel 产物",
    "Post Check out repository": "清理代码检出环境",
    "Post Set up Python": "清理 Python 环境",
    "Post Set up QEMU for Linux aarch64 wheels": "清理 QEMU 环境",
    "Complete job": "任务收尾",
}


def highlight(text, result, stream=None):
    stream = sys.stdout if stream is None else stream
    colors = {"in_progress": "1;33", "success": "1;32", "failure": "1;31", "timed_out": "1;31",
              "startup_failure": "1;31", "action_required": "1;31", "cancelled": "1;33"}
    color = colors.get(result)
    if (not color or not stream.isatty() or "NO_COLOR" in os.environ
            or os.environ.get("TERM") == "dumb"):
        return text
    return "\033[{}m{}\033[0m".format(color, text)


class ProgressDisplay:
    """Refresh a bounded terminal panel; keep redirected output as plain logs."""

    def __init__(self):
        self.stream = sys.stdout
        self.live = (self.stream.isatty() and os.environ.get("TERM") != "dumb"
                     and (os.name != "nt" or "WT_SESSION" in os.environ
                          or "ANSICON" in os.environ))
        self.rows = 0
        self.size = None

    @staticmethod
    def clip(line, columns):
        # Preserve SGR colors while counting Chinese characters as two cells.
        tokens = re.findall(r"\x1b\[[0-9;]*m|[^\x1b]", line)
        result = []
        width = 0
        for token in tokens:
            if token.startswith("\033["):
                result.append(token)
                continue
            if unicodedata.category(token).startswith("C"):
                continue
            cells = 0 if unicodedata.combining(token) else (
                2 if unicodedata.east_asian_width(token) in ("W", "F") else 1)
            if width + cells > columns - 1:
                return "".join(result) + "…\033[0m"
            result.append(token)
            width += cells
        return "".join(result)

    def update(self, text, final=False):
        if not self.live:
            print(text, file=self.stream, flush=True)
            return
        size = shutil.get_terminal_size()
        # After a resize, wrapped rows cannot be located reliably. Start anew.
        if self.rows and size == self.size:
            self.stream.write("\033[{}A\r\033[J".format(self.rows))
        lines = text.splitlines()
        if not final:
            lines = [self.clip(line, max(2, size.columns - 1))
                     for line in lines[:max(1, size.lines - 1)]]
        self.stream.write("\n".join(lines) + "\n")
        self.stream.flush()
        self.rows = 0 if final else len(lines)
        self.size = size


def duration(seconds):
    seconds = max(0, int(seconds))
    return "{}分{:02d}秒".format(seconds // 60, seconds % 60)


def elapsed_since(start, end=None):
    if not start:
        return "未知"
    try:
        began = datetime.fromisoformat(start.replace("Z", "+00:00"))
        ended = datetime.fromisoformat(end.replace("Z", "+00:00")) if end else datetime.now(timezone.utc)
        return duration((ended - began).total_seconds())
    except (TypeError, ValueError):
        return "未知"


def fetch_jobs(repo, run_id, attempt):
    jobs = []
    page = 1
    while True:
        report = api("repos/{}/actions/runs/{}/attempts/{}/jobs?per_page=100&page={}".format(
            repo, run_id, attempt, page))
        batch = report["jobs"]
        jobs.extend(batch)
        if len(jobs) >= report["total_count"] or not batch:
            return jobs
        page += 1


def artifacts_for_jobs(jobs):
    # Runs from before the native ARM split have one combined Linux artifact.
    if any(job["name"] == "Build ubuntu-22.04 wheels" for job in jobs):
        return {name: ("x86_64", "aarch64") if name.endswith("ubuntu-22.04") else archs
                for name, archs in ARTIFACTS.items() if not name.endswith("ubuntu-22.04-arm")}
    return ARTIFACTS


def format_progress(jobs, waited):
    # Each platform has equal weight. Jobs not yet materialized count as 0%.
    total_jobs = max(len(artifacts_for_jobs(jobs)), len(jobs))
    finished = sum(job["status"] == "completed" for job in jobs)
    succeeded = sum(job.get("conclusion") == "success" for job in jobs)
    fractions = []
    lines = []
    for job in sorted(jobs, key=lambda job: job["name"]):
        steps = job.get("steps") or []
        done = sum(step["status"] == "completed" for step in steps)
        complete = job["status"] == "completed"
        fraction = 1.0 if complete else done / len(steps) if steps else 0.0
        # A running job with only completed steps may still be registering cleanup.
        fractions.append(fraction if complete else min(fraction, 0.99))
        name = job["name"]
        for os_name, label in (("ubuntu-22.04-arm", "Linux aarch64"),
                               ("ubuntu-22.04", "Linux x86_64/aarch64" if name == "Build ubuntu-22.04 wheels" else "Linux x86_64"),
                               ("windows-2022", "Windows AMD64"), ("macos-14", "macOS arm64")):
            if os_name in name:
                name = label
                break
        status = highlight(STATUS_LABELS.get(job["status"], job["status"]), job["status"])
        conclusion = job.get("conclusion")
        if conclusion:
            status += " / " + highlight(STATUS_LABELS.get(conclusion, conclusion), conclusion)
        details = "步骤 {}/{}".format(done, len(steps)) if steps else "等待步骤信息"
        active = [step for step in steps if step["status"] == "in_progress"]
        if active:
            step = active[0]
            details += "；当前：{}（已耗时 {}）".format(
                STEP_LABELS.get(step["name"], step["name"]), elapsed_since(step.get("started_at")))
        elif complete and conclusion not in ("success", "skipped"):
            failed = [s for s in steps if s.get("conclusion") not in (None, "success", "skipped")]
            if failed:
                details += "；异常步骤：" + STEP_LABELS.get(failed[0]["name"], failed[0]["name"])
        lines.append("  {}：{}；{}；任务耗时 {}".format(
            name, status, details, elapsed_since(job.get("started_at"), job.get("completed_at"))))
        if complete and conclusion not in ("success", "skipped") and job.get("html_url"):
            lines.append("    任务详情：" + job["html_url"])
    percent = int(sum(fractions) / total_jobs * 100)
    summary = "构建进度（按步骤估算）：{}%；已结束任务 {}/{}，成功 {}；已等待 {}".format(
        percent, finished, total_jobs, succeeded, duration(waited))
    if len(jobs) < total_jobs:
        lines.append("  另有 {} 个平台任务等待 GitHub 提供详情。".format(total_jobs - len(jobs)))
    return "\n".join([summary, *lines])


def transient_query_error(error):
    if isinstance(error, (OSError, subprocess.TimeoutExpired)):
        return True
    message = str(error).lower()
    return any(marker in message for marker in (
        "connection reset", "connection refused", "connection closed", "unexpected eof",
        "i/o timeout", "timed out", "timeout", "命令执行超时", "tls handshake",
        "temporary failure", "no such host", "network is unreachable",
        "http 429", "http 500", "http 502", "http 503", "http 504",
    ))


def wait_for_next_poll(deadline, interval, run_id):
    remaining = deadline - time.monotonic()
    if remaining <= 0:
        raise RuntimeError("等待查询结果超时，远程构建不会因此取消。可使用 --run-id {} 恢复".format(run_id))
    time.sleep(min(interval, remaining))


def wait_for_run(repo, run_id, timeout, interval):
    started = time.monotonic()
    deadline = started + timeout
    previous = None
    progress_error = None
    status_error = None
    display = ProgressDisplay()
    print("进度按各平台已结束步骤的比例估算（含跳过或失败步骤），不代表实际编译量或剩余时间。", flush=True)
    while True:
        try:
            run = api("repos/{}/actions/runs/{}".format(repo, run_id))
            status_error = None
        except (RuntimeError, OSError, subprocess.TimeoutExpired) as error:
            if not transient_query_error(error):
                raise
            if display.live or str(error) != status_error:
                display.update("暂时无法获取构建状态，将自动重试（已等待 {}）：{}".format(
                    duration(time.monotonic() - started), error))
            status_error = str(error)
            wait_for_next_poll(deadline, interval, run_id)
            continue
        if run.get("path", "").split("@", 1)[0] != ".github/workflows/" + WORKFLOW:
            raise RuntimeError("运行 {} 不属于 BPX SDK wheels 工作流".format(run_id))
        state = (run["status"], run.get("conclusion"))
        status = highlight(STATUS_LABELS.get(state[0], state[0]), state[0])
        conclusion = highlight(STATUS_LABELS.get(state[1], state[1]), state[1])
        status_line = "构建状态：{}{}".format(status, " / " + conclusion if conclusion else "")
        if not display.live and state != previous:
            print(status_line, flush=True)
        previous = state
        progress = None
        jobs = []
        try:
            jobs = fetch_jobs(repo, run_id, run.get("run_attempt", 1))
            progress = format_progress(jobs, time.monotonic() - started)
            progress_error = None
        except (RuntimeError, OSError, ValueError, KeyError, TypeError, subprocess.TimeoutExpired) as error:
            if display.live or str(error) != progress_error:
                progress = "暂时无法获取任务进度，将继续等待构建：{}".format(error)
            progress_error = str(error)
        if display.live:
            display.update(status_line + ("\n" + progress if progress else ""),
                           final=run["status"] == "completed")
        elif progress:
            display.update(progress)
        if run["status"] == "completed":
            if run.get("conclusion") != "success":
                raise RuntimeError("构建未成功，目标目录未被修改。")
            if jobs:
                print("构建对应的提交：" + run["head_sha"], flush=True)
                return artifacts_for_jobs(jobs)
        wait_for_next_poll(deadline, interval, run_id)


def collect_wheels(download_dir, artifacts=None):
    """Validate all platform artifacts before updating the wheel output directory."""
    artifacts = ARTIFACTS if artifacts is None else artifacts
    files = {}
    versions = set()
    for artifact, architectures in artifacts.items():
        root = download_dir / artifact
        if not root.is_dir() or root.is_symlink():
            raise RuntimeError("缺少 wheel 产物：" + artifact)
        for path in root.rglob("*"):
            if path.is_symlink():
                raise RuntimeError("产物中存在不支持的符号链接：" + str(path))
        wheels = sorted(root.glob("*.whl"))
        if not wheels:
            raise RuntimeError("产物中没有 wheel 文件：" + artifact)
        platforms = set()
        for path in wheels:
            parts = path.name[:-4].split("-")
            if len(parts) not in (5, 6) or parts[0] != "bpx_sdk_open":
                raise RuntimeError("wheel 文件名或包名不符：" + path.name)
            versions.add(parts[1])
            platforms.update(parts[-1].split("."))
            try:
                with zipfile.ZipFile(path) as wheel:
                    if wheel.testzip() is not None:
                        raise RuntimeError("wheel 校验失败：" + path.name)
                    info = "bpx_sdk_open-" + parts[1] + ".dist-info/"
                    for name in ("WHEEL", "METADATA", "RECORD"):
                        if not wheel.read(info + name):
                            raise RuntimeError("wheel 元数据为空：" + path.name + " / " + name)
            except (zipfile.BadZipFile, KeyError, EOFError) as error:
                raise RuntimeError("wheel 损坏或缺少元数据：" + path.name) from error
            relative = Path(path.name)
            if relative in files and files[relative].read_bytes() != path.read_bytes():
                raise RuntimeError("不同产物存在同名但内容不同的 wheel：" + path.name)
            files[relative] = path
        for arch in architectures:
            if artifact.endswith(("ubuntu-22.04", "ubuntu-22.04-arm")):
                covered = any(p.startswith("manylinux") and p.endswith("_" + arch) for p in platforms)
            elif artifact.endswith("macos-14"):
                covered = any(p.startswith("macosx_") and p.endswith("_" + arch) for p in platforms)
            else:
                covered = arch in platforms
            if not covered:
                raise RuntimeError("wheel 产物缺少目标平台：" + artifact + " / " + arch)
    if len(versions) != 1:
        raise RuntimeError("三端 wheel 包版本不一致")
    return files


def install_package(files, prefix):
    # Check every destination before overwriting anything; preserve other files.
    for relative in files:
        target = prefix / relative
        for parent in (target.parent, *target.parent.parents):
            if parent == prefix.parent:
                break
            if parent.is_symlink() or (parent.exists() and not parent.is_dir()):
                raise RuntimeError("目标目录无效：" + str(parent))
        if target.is_symlink() or (target.exists() and not target.is_file()):
            raise RuntimeError("目标文件无效：" + str(target))
    for relative, source in files.items():
        target = prefix / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        # Replace individual wheels atomically.
        with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as temp:
            temporary = Path(temp.name)
        try:
            shutil.copy2(source, temporary)
            os.replace(temporary, target)
        finally:
            if temporary.exists():
                temporary.unlink()


def positive_int(value):
    try:
        number = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("必须是正整数") from error
    if number <= 0:
        raise argparse.ArgumentTypeError("必须是正整数")
    return number


def detect_repository():
    """Use origin so a fork builds its own workflow, not the upstream repository."""
    try:
        remote = command(["git", "remote", "get-url", "origin"]).strip().rstrip("/")
    except (RuntimeError, OSError) as error:
        raise RuntimeError("无法读取 origin，请通过 --repo OWNER/REPO 指定 GitHub 仓库。") from error
    match = re.fullmatch(
        r"(?:git@github\.com:|https://github\.com/|ssh://git@github\.com/)"
        r"([A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+)", remote)
    if not match:
        raise RuntimeError("无法从 origin 识别 github.com 仓库，请通过 --repo OWNER/REPO 显式指定。")
    repo = match.group(1)
    return repo[:-4] if repo.endswith(".git") else repo


def main(argv=None):
    parser = ChineseArgumentParser(description=__doc__, add_help=False)
    parser.add_argument("-h", "--help", action="help", help="显示帮助并退出")
    parser.add_argument("--out-dir", type=Path, default=REPO_ROOT / "wheelhouse",
                        help="wheel 输出目录（默认：仓库下的 wheelhouse）")
    parser.add_argument("--usage-only", action="store_true",
                        help="仅显示本月 Actions 用量，不触发构建或下载")
    parser.add_argument("--show-usage", action="store_true",
                        help="下载成功后查询账户用量（默认不查询，需要相应账单权限）")
    parser.add_argument("--repo",
                        help="GitHub 仓库 OWNER/REPO（默认：从本仓库 origin 自动识别）")
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--ref", help="要构建的远程分支或标签（默认：当前本地分支名）")
    source.add_argument("--run-id", type=positive_int,
                        help="等待并下载已有运行的产物，不触发新构建")
    parser.add_argument("--timeout", type=positive_int, default=7200,
                        help="等待构建的最长时间，单位秒（默认：%(default)s）")
    parser.add_argument("--poll-interval", type=positive_int, default=10,
                        help="查询构建状态的间隔，单位秒（默认：%(default)s）")
    args = parser.parse_args(argv)
    args.repo = args.repo or detect_repository()
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", args.repo):
        parser.error("--repo 必须是 github.com 上的 OWNER/REPO")
    prefix = args.out_dir.expanduser().resolve()
    if prefix and prefix.exists() and not prefix.is_dir():
        parser.error("--out-dir 必须指向目录")
    if not shutil.which("gh"):
        raise RuntimeError("未找到 gh；请安装 GitHub CLI 并执行 gh auth login")

    if args.usage_only:
        return 0 if show_usage(args.repo) else 1

    run_id = args.run_id
    if not run_id:
        ref = args.ref or command(["git", "branch", "--show-current"])
        if not ref:
            parser.error("当前处于 detached HEAD 状态，请通过 --ref 指定远程分支或标签")
        print("触发构建需要目标仓库写权限；下载已有构建可使用 --run-id。", flush=True)
        print("正在触发 {}/{}，远程分支或标签：{}".format(args.repo, WORKFLOW, ref), flush=True)
        run_id = dispatch(args.repo, ref)
    print("运行详情：https://github.com/{}/actions/runs/{}".format(args.repo, run_id), flush=True)
    print("如需恢复，请使用 --run-id {}，并保持 --repo / --out-dir 参数不变。".format(run_id), flush=True)
    artifacts = wait_for_run(args.repo, run_id, args.timeout, args.poll_interval)

    with tempfile.TemporaryDirectory(prefix="bpx-wheels-github-") as directory:
        download_dir = Path(directory)
        for artifact in artifacts:
            print("正在下载并解压：" + artifact, flush=True)
            command([
                "gh", "run", "download", str(run_id), "--repo", "github.com/" + args.repo,
                "--name", artifact, "--dir", str(download_dir / artifact),
            ], timeout=600)
        files = collect_wheels(download_dir, artifacts)
        install_package(files, prefix)
    print(highlight("已将 {} 个 wheel 文件保存到 {}".format(len(files), prefix), "success"), flush=True)
    if args.show_usage:
        print("正在查询用量；GitHub 账单可能有延迟，本次构建消耗可能尚未计入。", flush=True)
        if not show_usage(args.repo):
            print("wheel 已保存成功，但未能查询到最新用量。", flush=True)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, OSError, ValueError, subprocess.TimeoutExpired) as error:
        print(highlight("错误：" + str(error), "failure", sys.stderr), file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n已中断；已触发的远程构建仍会继续。可使用上方运行 ID 恢复。", file=sys.stderr)
        sys.exit(130)
