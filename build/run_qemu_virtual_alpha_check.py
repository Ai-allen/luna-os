from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parent
REPO_ROOT = ROOT.parent
ARTIFACTS_DIR = REPO_ROOT / "artifacts" / "lunaos"
STEP_LOG_DIR = ARTIFACTS_DIR / "virtual_alpha_steps"
SUMMARY_PATH = ARTIFACTS_DIR / "virtual_alpha_summary.json"
REPORT_PATH = ARTIFACTS_DIR / "virtual_alpha_report.md"


@dataclass(frozen=True)
class Step:
    name: str
    command: list[str]
    optional_group: str | None = None


def find_powershell() -> str:
    for candidate in ("pwsh.exe", "powershell.exe"):
        found = shutil.which(candidate)
        if found:
            return found
    raise FileNotFoundError("PowerShell not found.")


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


def run_step(step: Step) -> dict[str, object]:
    started = time.time()
    started_at = datetime.now(timezone.utc).isoformat()
    proc = subprocess.run(
        step.command,
        cwd=ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    finished_at = datetime.now(timezone.utc).isoformat()
    output = proc.stdout
    if proc.stderr:
        output += proc.stderr
    duration = round(time.time() - started, 3)
    return {
        "name": step.name,
        "command": step.command,
        "status": "passed" if proc.returncode == 0 else "failed",
        "exit_code": proc.returncode,
        "started_at": started_at,
        "finished_at": finished_at,
        "duration_seconds": duration,
        "output": output,
        "key_error": "" if proc.returncode == 0 else key_error_line(output),
    }


def write_report(summary: dict[str, object]) -> None:
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
        "| step | status | duration_s | log | key_error |",
        "| --- | --- | ---: | --- | --- |",
    ]
    for step in summary["steps"]:
        lines.append(
            f"| `{step['name']}` | `{step['status']}` | `{step.get('duration_seconds', 0)}` | `{step.get('output_path', '')}` | `{step.get('key_error', '')}` |"
        )
    REPORT_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")


def build_steps(include_vmware: bool) -> list[Step]:
    powershell = find_powershell()
    steps = [
        Step("build", [sys.executable, str(ROOT / "build.py")]),
        Step("bootcheck", [powershell, "-NoProfile", "-File", str(ROOT / "run_qemu_bootcheck.ps1")]),
        Step("shellcheck", [sys.executable, str(ROOT / "run_qemu_shellcheck.py")]),
        Step("desktopcheck", [sys.executable, str(ROOT / "run_qemu_desktopcheck.py")]),
        Step("uefi-shellcheck", [sys.executable, str(ROOT / "run_qemu_uefi_shellcheck.py")]),
        Step("uefi-stabilitycheck", [sys.executable, str(ROOT / "run_qemu_uefi_stabilitycheck.py")]),
        Step("userinputcheck", [sys.executable, str(ROOT / "run_qemu_userinputcheck.py")]),
        Step("desktopinputcheck", [sys.executable, str(ROOT / "run_qemu_desktopinputcheck.py")]),
        Step("usbhidcheck", [sys.executable, str(ROOT / "run_qemu_usbhidcheck.py")]),
        Step("securitycheck", [sys.executable, str(ROOT / "run_qemu_securitycheck.py")]),
        Step("inboundcheck", [sys.executable, str(ROOT / "run_qemu_inboundcheck.py")]),
        Step("externalstackcheck", [sys.executable, str(ROOT / "run_qemu_externalstackcheck.py")]),
        Step("updateapplycheck", [sys.executable, str(ROOT / "run_qemu_updateapplycheck.py")]),
        Step("updaterollbackcheck", [sys.executable, str(ROOT / "run_qemu_updaterollbackcheck.py")]),
        Step("recoverycheck", [sys.executable, str(ROOT / "run_qemu_recoverycheck.py")]),
        Step("lsysfailurecheck", [sys.executable, str(ROOT / "run_qemu_lsysfailurecheck.py")]),
        Step("installer-applycheck", [sys.executable, str(ROOT / "run_qemu_installer_applycheck.py")]),
        Step("installer-failurecheck", [sys.executable, str(ROOT / "run_qemu_installer_failurecheck.py")]),
        Step("fullregression", [sys.executable, str(ROOT / "run_qemu_fullregression.py")]),
    ]
    vmware_steps = [
        Step("vmware-desktopcheck", [powershell, "-NoProfile", "-File", str(ROOT / "run_vmware_desktopcheck.ps1")], "vmware"),
        Step("vmware-inputcheck", [powershell, "-NoProfile", "-File", str(ROOT / "run_vmware_inputcheck.ps1")], "vmware"),
    ]
    if include_vmware:
        steps.extend(vmware_steps)
    else:
        steps.extend(vmware_steps)
    return steps


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the LunaOS Virtual Complete Alpha gate.")
    parser.add_argument(
        "--include-vmware",
        action="store_true",
        help="Include VMware desktop and real-keyboard gates. Disabled by default.",
    )
    args = parser.parse_args()

    ARTIFACTS_DIR.mkdir(parents=True, exist_ok=True)
    STEP_LOG_DIR.mkdir(parents=True, exist_ok=True)

    summary: dict[str, object] = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "repo_root": str(REPO_ROOT),
        "include_vmware": bool(args.include_vmware),
        "status": "passed",
        "steps": [],
    }

    overall_start = time.time()
    for step in build_steps(args.include_vmware):
        if step.optional_group == "vmware" and not args.include_vmware:
            output_path = STEP_LOG_DIR / f"{step.name}.log"
            output_path.write_text("skipped: pass --include-vmware to run this gate\n", encoding="utf-8")
            result = {
                "name": step.name,
                "command": step.command,
                "status": "skipped",
                "exit_code": None,
                "duration_seconds": 0,
                "output_path": str(output_path),
                "key_error": "",
                "skip_reason": "requires --include-vmware",
            }
            summary["steps"].append(result)
            print(f"==> {step.name} skipped (--include-vmware not set)")
            continue

        print(f"==> {step.name}")
        result = run_step(step)
        output_path = STEP_LOG_DIR / f"{step.name}.log"
        output_path.write_text(str(result["output"]), encoding="utf-8", errors="replace")
        result["output_path"] = str(output_path)
        result_without_output = {key: value for key, value in result.items() if key != "output"}
        summary["steps"].append(result_without_output)
        sys.stdout.buffer.write(str(result["output"]).encode("utf-8", errors="replace"))
        if result["status"] != "passed":
            summary["status"] = "failed"

    summary["duration_seconds"] = round(time.time() - overall_start, 3)
    summary["total_steps"] = len(summary["steps"])
    summary["passed_steps"] = sum(1 for step in summary["steps"] if step["status"] == "passed")
    summary["failed_steps"] = sum(1 for step in summary["steps"] if step["status"] == "failed")
    summary["skipped_steps"] = sum(1 for step in summary["steps"] if step["status"] == "skipped")

    SUMMARY_PATH.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    write_report(summary)

    print("")
    gate_result = "pass" if summary["status"] == "passed" else "fail"
    print(f"Virtual Complete Alpha: {gate_result}")
    print(f"total steps: {summary['total_steps']}")
    print(f"passed steps: {summary['passed_steps']}")
    print(f"failed steps: {summary['failed_steps']}")
    print(f"skipped steps: {summary['skipped_steps']}")
    print("step logs:")
    for step in summary["steps"]:
        print(f"- {step['name']}: {step['status']} log={step.get('output_path', '')}")
    failed = [step for step in summary["steps"] if step["status"] == "failed"]
    if failed:
        print("failed key errors:")
        for step in failed:
            print(f"- {step['name']}: {step.get('key_error', 'no output captured')}")
    else:
        print("failed key errors: none")

    return 0 if summary["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
