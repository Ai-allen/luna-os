from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import time
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path

from run_qemu_virtual_alpha_check import Step, build_steps


ROOT = Path(__file__).resolve().parent
REPO_ROOT = ROOT.parent
ARTIFACTS_DIR = REPO_ROOT / "artifacts" / "lunaos"
STABILITY_DIR = ARTIFACTS_DIR / "virtual_alpha_stability"
RUNS_DIR = STABILITY_DIR / "runs"
SUMMARY_PATH = STABILITY_DIR / "summary.json"
REPORT_PATH = STABILITY_DIR / "report.md"

VIRTUAL_ALPHA_SUMMARY = ARTIFACTS_DIR / "virtual_alpha_summary.json"
VIRTUAL_ALPHA_REPORT = ARTIFACTS_DIR / "virtual_alpha_report.md"
VIRTUAL_ALPHA_STEPS = ARTIFACTS_DIR / "virtual_alpha_steps"

INTERNAL_LOG_PATTERNS: dict[str, tuple[str, ...]] = {
    "updateapplycheck": (
        "qemu_updateapplycheck_boot*.log",
        "qemu_updateapplycheck_boot*.err.log",
    ),
    "updaterollbackcheck": (
        "qemu_updaterollbackcheck_boot*.log",
        "qemu_updaterollbackcheck_boot*.err.log",
    ),
    "usbhidcheck": (
        "qemu_usbhidcheck.log",
        "qemu_usbhidcheck.err.log",
    ),
    "installer-applycheck": (
        "qemu_installer_applycheck.log",
        "qemu_installer_applycheck.err.log",
        "qemu_installer_target_bootcheck.log",
        "qemu_installer_target_bootcheck.err.log",
    ),
    "installer-failurecheck": (
        "qemu_installer_failurecheck.log",
        "qemu_installer_failurecheck.err.log",
        "qemu_installer_failure_recovery.log",
        "qemu_installer_failure_recovery.err.log",
        "qemu_installer_idempotency.log",
        "qemu_installer_idempotency.err.log",
        "qemu_installer_failure_bootcheck.log",
        "qemu_installer_failure_bootcheck.err.log",
    ),
}

STEP_IMAGE_PATHS: dict[str, tuple[str, ...]] = {
    "updateapplycheck": ("lunaos.img", "lunaos_updateapplycheck.img"),
    "updaterollbackcheck": ("lunaos.img", "lunaos_updaterollbackcheck.img"),
    "usbhidcheck": ("lunaos.img", "lunaos_usbhidcheck.img"),
    "installer-applycheck": (
        "lunaos_installer.iso",
        "lunaos_disk.img",
        "qemu_installer_target_apply.img",
    ),
    "installer-failurecheck": (
        "lunaos_installer.iso",
        "lunaos_disk.img",
        "qemu_installer_target_failure.img",
    ),
}

STEP_OVMF_PATHS: dict[str, tuple[str, ...]] = {
    "usbhidcheck": ("ovmf-usbhidcheck-vars.fd",),
    "installer-applycheck": (
        "ovmf-installer-target-vars.fd",
        "ovmf-installer-target-boot-vars.fd",
    ),
    "installer-failurecheck": (
        "ovmf-installer-failure-vars.fd",
        "ovmf-installer-recovery-vars.fd",
        "ovmf-installer-idempotency-vars.fd",
        "ovmf-installer-failure-boot-vars.fd",
    ),
}


def classify_failure(step_name: str, key_error: str) -> str:
    text = f"{step_name} {key_error}".lower()
    if "usb" in text or "hid" in text or "xhci" in text or "erdp" in text:
        return "USB-HID polling instability"
    if "inbound" in text or "externalstack" in text or "network" in text or "net." in text:
        return "network/inbound instability"
    if "update" in text or "rollback" in text or "recovery" in text or "install" in text:
        return "update/recovery instability"
    if "cannot open vm" in text or "permission" in text or "timed out" in text or "resource" in text:
        return "host resource failure"
    if "missing ordered" in text or "missing expected" in text or "qmp" in text or "sendkey" in text:
        return "harness timing failure"
    return "QEMU guest failure"


def load_virtual_alpha_summary() -> dict[str, object]:
    if not VIRTUAL_ALPHA_SUMMARY.exists():
        return {
            "status": "failed",
            "steps": [],
            "failed_steps": 1,
            "key_error": "virtual alpha summary was not generated",
        }
    return json.loads(VIRTUAL_ALPHA_SUMMARY.read_text(encoding="utf-8"))


def first_failed_step(summary: dict[str, object]) -> dict[str, object] | None:
    for step in summary.get("steps", []):
        if isinstance(step, dict) and step.get("status") == "failed":
            return step
    return None


def key_error_line(output: str) -> str:
    markers = (
        "RuntimeError",
        "Traceback",
        "Exception",
        "missing expected",
        "observed forbidden",
        "observed unexpected",
        "not found",
        "failed",
        "fail",
        "Error",
        "error:",
    )
    lines = [line.strip() for line in output.splitlines() if line.strip()]
    for line in reversed(lines):
        lowered = line.lower()
        if any(marker.lower() in lowered for marker in markers):
            return line
    if lines:
        return lines[-1]
    return "no output captured"


def marker_context(output: str, marker: str, context: int = 8) -> list[str]:
    if not marker:
        return []
    lines = output.splitlines()
    for index, line in enumerate(lines):
        if marker in line:
            start = max(0, index - context)
            end = min(len(lines), index + context + 1)
            return lines[start:end]
    return lines[-context:] if lines else []


def qemu_process_snapshot() -> dict[str, object]:
    try:
        proc = subprocess.run(
            ["tasklist", "/FI", "IMAGENAME eq qemu-system-x86_64.exe", "/FO", "CSV", "/NH"],
            cwd=ROOT,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    except OSError as exc:
        return {"available": False, "error": str(exc), "pids": [], "raw": ""}
    raw = proc.stdout.strip()
    pids: list[int] = []
    for line in raw.splitlines():
        line = line.strip()
        if not line or "INFO:" in line:
            continue
        parts = [part.strip().strip('"') for part in line.split('","')]
        if len(parts) >= 2:
            try:
                pids.append(int(parts[1]))
            except ValueError:
                pass
    return {
        "available": True,
        "return_code": proc.returncode,
        "pids": pids,
        "count": len(pids),
        "raw": raw,
    }


def path_snapshot(names: tuple[str, ...]) -> list[dict[str, object]]:
    paths: list[dict[str, object]] = []
    for name in names:
        path = ROOT / name
        paths.append(
            {
                "path": str(path),
                "exists": path.exists(),
                "size_bytes": path.stat().st_size if path.exists() and path.is_file() else None,
            }
        )
    return paths


def copy_matching_logs(step_name: str, dest_dir: Path) -> list[str]:
    dest_dir.mkdir(parents=True, exist_ok=True)
    copied: list[str] = []
    seen_sources: set[Path] = set()
    patterns = list(INTERNAL_LOG_PATTERNS.get(step_name, ()))
    normalized = step_name.replace("-", "_")
    patterns.extend((f"qemu_{normalized}*.log", f"qemu_{normalized}*.err.log"))
    for pattern in dict.fromkeys(patterns):
        for source in sorted(ROOT.glob(pattern)):
            if not source.is_file():
                continue
            source = source.resolve()
            if source in seen_sources:
                continue
            seen_sources.add(source)
            target = dest_dir / source.name
            shutil.copyfile(source, target)
            copied.append(str(target))
    if step_name == "fullregression":
        nested_patterns = (
            "qemu_updateapplycheck_boot*.log",
            "qemu_updateapplycheck_boot*.err.log",
            "qemu_updaterollbackcheck_boot*.log",
            "qemu_updaterollbackcheck_boot*.err.log",
            "qemu_installer_failure*.log",
            "qemu_installer_failure*.err.log",
        )
        for pattern in nested_patterns:
            for source in sorted(ROOT.glob(pattern)):
                if not source.is_file():
                    continue
                source = source.resolve()
                if source in seen_sources:
                    continue
                seen_sources.add(source)
                target = dest_dir / source.name
                shutil.copyfile(source, target)
                copied.append(str(target))
        for source in (
            ARTIFACTS_DIR / "fullregression_summary.json",
            ARTIFACTS_DIR / "fullregression_report.md",
            ARTIFACTS_DIR / "fullregression_junit.xml",
        ):
            if source.exists() and source.is_file():
                target = dest_dir / source.name
                shutil.copyfile(source, target)
                copied.append(str(target))
        source_dir = ARTIFACTS_DIR / "fullregression_steps"
        if source_dir.exists():
            target_dir = dest_dir / "fullregression_steps"
            if target_dir.exists():
                shutil.rmtree(target_dir)
            shutil.copytree(source_dir, target_dir)
            copied.append(str(target_dir))
    return copied


def write_virtual_alpha_report(summary: dict[str, object], report_path: Path) -> None:
    lines = [
        "# LunaOS Virtual Complete Alpha Gate",
        "",
        f"- generated_at: `{summary['generated_at']}`",
        f"- status: `{summary['status']}`",
        f"- total_steps: `{summary['total_steps']}`",
        f"- passed_steps: `{summary['passed_steps']}`",
        f"- failed_steps: `{summary['failed_steps']}`",
        f"- skipped_steps: `{summary['skipped_steps']}`",
        f"- duration_seconds: `{summary['duration_seconds']}`",
        "",
        "## Steps",
        "",
        "| step | status | duration_s | log | key_error | internal_logs |",
        "| --- | --- | ---: | --- | --- | --- |",
    ]
    for step in summary["steps"]:
        internal_count = len(step.get("internal_logs", []))
        lines.append(
            f"| `{step['name']}` | `{step['status']}` | `{step.get('duration_seconds', 0)}` | "
            f"`{step.get('output_path', '')}` | `{step.get('key_error', '')}` | `{internal_count}` |"
        )
    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def run_virtual_alpha_step(step: Step, run_dir: Path) -> dict[str, object]:
    step_log_dir = run_dir / "virtual_alpha_steps"
    internal_dir = run_dir / "internal_logs" / step.name
    metadata_dir = run_dir / "metadata"
    step_log_dir.mkdir(parents=True, exist_ok=True)
    metadata_dir.mkdir(parents=True, exist_ok=True)

    if step.optional_group == "vmware":
        output_path = step_log_dir / f"{step.name}.log"
        output_path.write_text("skipped: pass --include-vmware to run this gate\n", encoding="utf-8")
        return {
            "name": step.name,
            "command": step.command,
            "status": "skipped",
            "exit_code": None,
            "duration_seconds": 0,
            "output_path": str(output_path),
            "key_error": "",
            "skip_reason": "requires --include-vmware",
            "internal_logs": [],
            "metadata_path": "",
        }

    started = time.time()
    started_at = datetime.now(timezone.utc).isoformat()
    qemu_before = qemu_process_snapshot()
    proc = subprocess.run(
        step.command,
        cwd=ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    finished_at = datetime.now(timezone.utc).isoformat()
    duration = round(time.time() - started, 3)
    output = proc.stdout
    if proc.stderr:
        output += proc.stderr
    qemu_after = qemu_process_snapshot()
    key_error = "" if proc.returncode == 0 else key_error_line(output)
    output_path = step_log_dir / f"{step.name}.log"
    output_path.write_text(output, encoding="utf-8", errors="replace")
    internal_logs = copy_matching_logs(step.name, internal_dir)

    metadata = {
        "name": step.name,
        "command": step.command,
        "start_time": started_at,
        "finish_time": finished_at,
        "duration_seconds": duration,
        "exit_code": proc.returncode,
        "status": "passed" if proc.returncode == 0 else "failed",
        "failed_marker": key_error,
        "marker_context": marker_context(output, key_error),
        "image_paths": path_snapshot(STEP_IMAGE_PATHS.get(step.name, ())),
        "target_image_paths": path_snapshot(
            tuple(name for name in STEP_IMAGE_PATHS.get(step.name, ()) if "target" in name)
        ),
        "ovmf_vars_paths": path_snapshot(STEP_OVMF_PATHS.get(step.name, ())),
        "qemu_processes_before": qemu_before,
        "qemu_processes_after": qemu_after,
        "previous_qemu_process_existed": bool(qemu_before.get("pids")),
        "ports_or_sockets": [],
        "timeout_used": "owned by child check script",
        "internal_logs": internal_logs,
    }
    metadata_path = metadata_dir / f"{step.name}.json"
    metadata_path.write_text(json.dumps(metadata, indent=2), encoding="utf-8")

    return {
        "name": step.name,
        "command": step.command,
        "status": metadata["status"],
        "exit_code": proc.returncode,
        "started_at": started_at,
        "finished_at": finished_at,
        "duration_seconds": duration,
        "output_path": str(output_path),
        "key_error": key_error,
        "failed_marker": key_error,
        "marker_context": metadata["marker_context"],
        "internal_logs": internal_logs,
        "metadata_path": str(metadata_path),
        "previous_qemu_process_existed": metadata["previous_qemu_process_existed"],
    }


def run_virtual_alpha(run_index: int) -> dict[str, object]:
    run_name = f"run-{run_index:02d}"
    run_dir = RUNS_DIR / run_name
    started = time.time()
    started_at = datetime.now(timezone.utc).isoformat()
    run_dir.mkdir(parents=True, exist_ok=True)
    output_path = run_dir / "run_qemu_virtual_alpha_check.log"
    summary_path = run_dir / "virtual_alpha_summary.json"
    report_path = run_dir / "virtual_alpha_report.md"

    output_chunks: list[str] = []
    alpha_summary: dict[str, object] = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "repo_root": str(REPO_ROOT),
        "include_vmware": False,
        "status": "passed",
        "steps": [],
    }
    for step in build_steps(False):
        if step.optional_group == "vmware":
            print(f"    {step.name} skipped")
        else:
            print(f"    {step.name}")
        result = run_virtual_alpha_step(step, run_dir)
        alpha_summary["steps"].append(result)
        step_output = Path(str(result["output_path"])).read_text(encoding="utf-8", errors="replace")
        output_chunks.append(f"==> {step.name}\n")
        output_chunks.append(step_output)
        if result["status"] != "passed" and result["status"] != "skipped":
            alpha_summary["status"] = "failed"

    finished_at = datetime.now(timezone.utc).isoformat()
    duration = round(time.time() - started, 3)
    alpha_summary["duration_seconds"] = duration
    alpha_summary["total_steps"] = len(alpha_summary["steps"])
    alpha_summary["passed_steps"] = sum(1 for step in alpha_summary["steps"] if step["status"] == "passed")
    alpha_summary["failed_steps"] = sum(1 for step in alpha_summary["steps"] if step["status"] == "failed")
    alpha_summary["skipped_steps"] = sum(1 for step in alpha_summary["steps"] if step["status"] == "skipped")
    output_chunks.append("")
    gate_result = "pass" if alpha_summary["status"] == "passed" else "fail"
    output_chunks.append(f"Virtual Complete Alpha: {gate_result}\n")
    output_chunks.append(f"total steps: {alpha_summary['total_steps']}\n")
    output_chunks.append(f"passed steps: {alpha_summary['passed_steps']}\n")
    output_chunks.append(f"failed steps: {alpha_summary['failed_steps']}\n")
    output_chunks.append(f"skipped steps: {alpha_summary['skipped_steps']}\n")
    output_path.write_text("".join(output_chunks), encoding="utf-8", errors="replace")
    summary_path.write_text(json.dumps(alpha_summary, indent=2), encoding="utf-8")
    write_virtual_alpha_report(alpha_summary, report_path)
    failed = first_failed_step(alpha_summary)
    status = "passed" if alpha_summary.get("status") == "passed" else "failed"
    failed_step = ""
    key_error = ""
    failure_class = ""
    flaky_marker = ""
    if failed is not None:
        failed_step = str(failed.get("name", "unknown"))
        key_error = str(failed.get("key_error", ""))
        failure_class = classify_failure(failed_step, key_error)
        flaky_marker = failed_step
    elif status != "passed":
        failed_step = "virtual-alpha"
        key_error = str(alpha_summary.get("key_error", "virtual alpha failed without failed step"))
        failure_class = classify_failure(failed_step, key_error)
        flaky_marker = failed_step

    return {
        "run": run_index,
        "name": run_name,
        "start_time": started_at,
        "finish_time": finished_at,
        "duration_seconds": duration,
        "status": status,
        "exit_code": 0 if status == "passed" else 1,
        "failed_step": failed_step,
        "key_error": key_error,
        "failure_class": failure_class,
        "flaky_marker": flaky_marker,
        "log_path": str(output_path),
        "summary_path": str(summary_path),
        "report_path": str(report_path),
        "steps_dir": str(run_dir / "virtual_alpha_steps"),
    }


def write_report(summary: dict[str, object]) -> None:
    lines = [
        "# LunaOS Virtual Alpha Reliability Stability Check",
        "",
        f"- generated_at: `{summary['generated_at']}`",
        f"- status: `{summary['status']}`",
        f"- total_runs: `{summary['total_runs']}`",
        f"- passed_runs: `{summary['passed_runs']}`",
        f"- failed_runs: `{summary['failed_runs']}`",
        f"- worst_duration_seconds: `{summary['worst_duration_seconds']}`",
        f"- logs_directory: `{summary['logs_directory']}`",
        "",
        "## Runs",
        "",
        "| run | status | duration_s | failed_step | failure_class | log |",
        "| --- | --- | ---: | --- | --- | --- |",
    ]
    for run in summary["runs"]:
        lines.append(
            f"| `{run['name']}` | `{run['status']}` | `{run['duration_seconds']}` | `{run['failed_step']}` | `{run['failure_class']}` | `{run['log_path']}` |"
        )
    lines.extend(["", "## Flaky Steps", ""])
    if summary["flaky_steps"]:
        for step, count in summary["flaky_steps"].items():
            lines.append(f"- `{step}`: `{count}`")
    else:
        lines.append("- none")
    REPORT_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the QEMU Virtual Alpha gate repeatedly and summarize reliability.")
    parser.add_argument("--runs", type=int, default=3, help="Number of QEMU Virtual Alpha runs. Default: 3.")
    args = parser.parse_args()
    if args.runs < 1:
        raise SystemExit("--runs must be at least 1")

    STABILITY_DIR.mkdir(parents=True, exist_ok=True)
    if RUNS_DIR.exists():
        shutil.rmtree(RUNS_DIR)
    RUNS_DIR.mkdir(parents=True, exist_ok=True)

    started = time.time()
    summary: dict[str, object] = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "repo_root": str(REPO_ROOT),
        "status": "passed",
        "runs": [],
        "logs_directory": str(RUNS_DIR),
    }

    for index in range(1, args.runs + 1):
        print(f"==> virtual-alpha reliability run {index}/{args.runs}")
        result = run_virtual_alpha(index)
        summary["runs"].append(result)
        print(
            f"{result['name']}: {result['status']} duration={result['duration_seconds']}s "
            f"failed_step={result['failed_step'] or '-'} log={result['log_path']}"
        )
        if result["status"] != "passed":
            summary["status"] = "failed"

    runs = list(summary["runs"])
    failed_runs = [run for run in runs if run["status"] != "passed"]
    flaky_counts = Counter(str(run["flaky_marker"]) for run in failed_runs if run["flaky_marker"])
    summary["duration_seconds"] = round(time.time() - started, 3)
    summary["total_runs"] = len(runs)
    summary["passed_runs"] = sum(1 for run in runs if run["status"] == "passed")
    summary["failed_runs"] = len(failed_runs)
    summary["flaky_steps"] = dict(sorted(flaky_counts.items()))
    summary["worst_duration_seconds"] = max((run["duration_seconds"] for run in runs), default=0)

    SUMMARY_PATH.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    write_report(summary)

    print("")
    result_word = "pass" if summary["status"] == "passed" else "fail"
    print(f"Virtual Alpha Reliability: {result_word}")
    print(f"total runs: {summary['total_runs']}")
    print(f"passed runs: {summary['passed_runs']}")
    print(f"failed runs: {summary['failed_runs']}")
    print(f"flaky steps: {', '.join(summary['flaky_steps'].keys()) if summary['flaky_steps'] else 'none'}")
    print(f"worst duration: {summary['worst_duration_seconds']}")
    print(f"logs directory: {summary['logs_directory']}")
    if failed_runs:
        print("failed runs:")
        for run in failed_runs:
            print(
                f"- {run['name']}: step={run['failed_step']} class={run['failure_class']} "
                f"error={run['key_error']} log={run['log_path']}"
            )

    return 0 if summary["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
