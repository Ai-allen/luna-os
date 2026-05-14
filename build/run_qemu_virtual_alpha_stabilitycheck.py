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


def copy_run_artifacts(run_dir: Path) -> dict[str, object]:
    run_dir.mkdir(parents=True, exist_ok=True)
    copied: dict[str, object] = {}
    if VIRTUAL_ALPHA_SUMMARY.exists():
        target = run_dir / "virtual_alpha_summary.json"
        shutil.copyfile(VIRTUAL_ALPHA_SUMMARY, target)
        copied["summary_path"] = str(target)
    if VIRTUAL_ALPHA_REPORT.exists():
        target = run_dir / "virtual_alpha_report.md"
        shutil.copyfile(VIRTUAL_ALPHA_REPORT, target)
        copied["report_path"] = str(target)
    if VIRTUAL_ALPHA_STEPS.exists():
        target = run_dir / "virtual_alpha_steps"
        if target.exists():
            shutil.rmtree(target)
        shutil.copytree(VIRTUAL_ALPHA_STEPS, target)
        copied["steps_dir"] = str(target)
    return copied


def run_virtual_alpha(run_index: int) -> dict[str, object]:
    run_name = f"run-{run_index:02d}"
    run_dir = RUNS_DIR / run_name
    started = time.time()
    started_at = datetime.now(timezone.utc).isoformat()
    proc = subprocess.run(
        [sys.executable, str(ROOT / "run_qemu_virtual_alpha_check.py")],
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

    run_dir.mkdir(parents=True, exist_ok=True)
    output_path = run_dir / "run_qemu_virtual_alpha_check.log"
    output_path.write_text(output, encoding="utf-8", errors="replace")
    alpha_summary = load_virtual_alpha_summary()
    copied = copy_run_artifacts(run_dir)
    failed = first_failed_step(alpha_summary)
    status = "passed" if proc.returncode == 0 and alpha_summary.get("status") == "passed" else "failed"
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
        "exit_code": proc.returncode,
        "failed_step": failed_step,
        "key_error": key_error,
        "failure_class": failure_class,
        "flaky_marker": flaky_marker,
        "log_path": str(output_path),
        **copied,
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
