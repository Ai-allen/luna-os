from __future__ import annotations

import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent


def run_step(label: str, *args: str) -> None:
    subprocess.run(
        [sys.executable, *args],
        cwd=ROOT.parent,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    print(f"firsthop suite ok: {label}")


def main() -> int:
    run_step("classifier-selftest", str(ROOT / "run_firsthop_classifier_selftest.py"))
    run_step(
        "baseline-legacy",
        str(ROOT / "select_firsthop_baseline.py"),
        str(ROOT / "qemu_bootcheck.log"),
    )
    run_step(
        "baseline-uefi",
        str(ROOT / "select_firsthop_baseline.py"),
        str(ROOT / "qemu_uefi_shellcheck.log"),
    )
    run_step(
        "verdict-legacy",
        str(ROOT / "render_firsthop_verdict.py"),
        str(ROOT / "qemu_bootcheck.log"),
    )
    run_step(
        "verdict-uefi",
        str(ROOT / "render_firsthop_verdict.py"),
        str(ROOT / "qemu_uefi_shellcheck.log"),
    )
    run_step("session-smoke", str(ROOT / "run_bringup_session_smoke.py"))
    print("firsthop suite complete")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
