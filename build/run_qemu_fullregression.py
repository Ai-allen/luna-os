from __future__ import annotations

import json
import shutil
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
from hashlib import sha256
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parent
REPO_ROOT = ROOT.parent
ARTIFACTS_DIR = REPO_ROOT / "artifacts" / "lunaos"
SUMMARY_PATH = ARTIFACTS_DIR / "fullregression_summary.json"
JUNIT_PATH = ARTIFACTS_DIR / "fullregression_junit.xml"
STEP_LOG_DIR = ARTIFACTS_DIR / "fullregression_steps"
REPORT_PATH = ARTIFACTS_DIR / "fullregression_report.md"


def find_powershell() -> str:
    for candidate in ("pwsh.exe", "powershell.exe"):
        found = shutil.which(candidate)
        if found:
            return found
    raise FileNotFoundError("PowerShell not found.")


def run_step(name: str, command: list[str], workdir: Path) -> dict[str, object]:
    started = time.time()
    started_at = datetime.now(timezone.utc).isoformat()
    proc = subprocess.run(
        command,
        cwd=workdir,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    finished_at = datetime.now(timezone.utc).isoformat()
    duration = round(time.time() - started, 3)
    combined = proc.stdout
    if proc.stderr:
        combined += proc.stderr

    return {
        "name": name,
        "command": command,
        "workdir": str(workdir),
        "started_at": started_at,
        "finished_at": finished_at,
        "duration_seconds": duration,
        "exit_code": proc.returncode,
        "status": "passed" if proc.returncode == 0 else "failed",
        "output": combined,
    }


def write_junit(summary: dict[str, object]) -> None:
    steps = list(summary["steps"])
    suite = ET.Element(
        "testsuite",
        name="lunaos-fullregression",
        tests=str(len(steps)),
        failures=str(sum(1 for step in steps if step["status"] != "passed")),
        errors="0",
        skipped="0",
        time=f"{summary['duration_seconds']:.3f}",
        timestamp=str(summary["generated_at"]),
    )

    for step in steps:
        case = ET.SubElement(
            suite,
            "testcase",
            name=str(step["name"]),
            classname="lunaos.vm",
            time=f"{step['duration_seconds']:.3f}",
        )
        ET.SubElement(case, "properties")
        if step["status"] != "passed":
            failure = ET.SubElement(
                case,
                "failure",
                message=f"{step['name']} failed with exit code {step['exit_code']}",
            )
            failure.text = f"log: {step['output_path']}"
        system_out = ET.SubElement(case, "system-out")
        system_out.text = f"log: {step['output_path']}"

    tree = ET.ElementTree(suite)
    ET.indent(tree, space="  ")
    tree.write(JUNIT_PATH, encoding="utf-8", xml_declaration=True)


def file_info(path: Path) -> dict[str, object]:
    digest = sha256(path.read_bytes()).hexdigest()
    return {
        "path": str(path),
        "size_bytes": path.stat().st_size,
        "sha256": digest,
    }


def write_markdown_report(summary: dict[str, object]) -> None:
    image_paths = [
        ROOT / "lunaos.img",
        ROOT / "lunaos_disk.img",
        ROOT / "lunaos_esp.img",
        ROOT / "EFI" / "BOOT" / "BOOTX64.EFI",
    ]
    artifacts = [file_info(path) for path in image_paths if path.exists()]
    summary["artifacts"] = artifacts

    lines = [
        "# LunaOS Full Regression Report",
        "",
        f"- generated_at: `{summary['generated_at']}`",
        f"- status: `{summary['status']}`",
        f"- passed_steps: `{summary['passed_steps']}`",
        f"- failed_steps: `{summary['failed_steps']}`",
        f"- total_steps: `{summary['total_steps']}`",
        f"- duration_seconds: `{summary['duration_seconds']}`",
        "",
        "## Steps",
        "",
        "| step | status | duration_s | log |",
        "| --- | --- | ---: | --- |",
    ]

    for step in summary["steps"]:
        lines.append(
            f"| `{step['name']}` | `{step['status']}` | `{step['duration_seconds']}` | `{step['output_path']}` |"
        )

    lines.extend(
        [
            "",
            "## Artifacts",
            "",
            "| path | size_bytes | sha256 |",
            "| --- | ---: | --- |",
        ]
    )

    for artifact in artifacts:
        lines.append(
            f"| `{artifact['path']}` | `{artifact['size_bytes']}` | `{artifact['sha256']}` |"
        )

    REPORT_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    powershell = find_powershell()
    ARTIFACTS_DIR.mkdir(parents=True, exist_ok=True)
    STEP_LOG_DIR.mkdir(parents=True, exist_ok=True)

    steps = [
        ("build", [sys.executable, str(ROOT / "build.py")], ROOT),
        ("bootcheck", [powershell, "-NoProfile", "-File", str(ROOT / "run_qemu_bootcheck.ps1")], ROOT),
        ("shellcheck", [sys.executable, str(ROOT / "run_qemu_shellcheck.py")], ROOT),
        ("desktopcheck", [sys.executable, str(ROOT / "run_qemu_desktopcheck.py")], ROOT),
        ("uefi-shellcheck", [sys.executable, str(ROOT / "run_qemu_uefi_shellcheck.py")], ROOT),
        ("uefi-stabilitycheck", [sys.executable, str(ROOT / "run_qemu_uefi_stabilitycheck.py")], ROOT),
        ("inboundcheck", [sys.executable, str(ROOT / "run_qemu_inboundcheck.py")], ROOT),
        ("externalstackcheck", [sys.executable, str(ROOT / "run_qemu_externalstackcheck.py")], ROOT),
        ("updateapplycheck", [sys.executable, str(ROOT / "run_qemu_updateapplycheck.py")], ROOT),
        ("updaterollbackcheck", [sys.executable, str(ROOT / "run_qemu_updaterollbackcheck.py")], ROOT),
        ("installer-failurecheck", [sys.executable, str(ROOT / "run_qemu_installer_failurecheck.py")], ROOT),
    ]

    summary: dict[str, object] = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "repo_root": str(REPO_ROOT),
        "status": "passed",
        "steps": [],
    }

    overall_start = time.time()
    for name, command, workdir in steps:
        print(f"==> {name}")
        result = run_step(name, command, workdir)
        output_path = STEP_LOG_DIR / f"{name}.log"
        output_path.write_text(str(result["output"]), encoding="utf-8")
        summary["steps"].append(
            {
                key: value
                for key, value in result.items()
                if key != "output"
            }
        )
        summary["steps"][-1]["output_path"] = str(output_path)
        sys.stdout.buffer.write(str(result["output"]).encode("utf-8", errors="replace"))
        if result["exit_code"] != 0:
            summary["status"] = "failed"
            break

    summary["duration_seconds"] = round(time.time() - overall_start, 3)
    passed = sum(1 for step in summary["steps"] if step["status"] == "passed")
    failed = sum(1 for step in summary["steps"] if step["status"] == "failed")
    summary["passed_steps"] = passed
    summary["failed_steps"] = failed
    summary["total_steps"] = len(steps)

    write_markdown_report(summary)
    SUMMARY_PATH.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    write_junit(summary)
    if summary["status"] != "passed":
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
