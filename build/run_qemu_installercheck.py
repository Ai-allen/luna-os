from __future__ import annotations

import os
import shutil
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent
REPO_ROOT = ROOT.parent
SOURCE_IMAGE = ROOT / "lunaos_disk.img"
TARGET_IMAGE = ROOT / "lunaos_installercheck.img"
LOG_PATH = ROOT / "qemu_installercheck.log"
ERR_PATH = ROOT / "qemu_installercheck.err.log"
INSTALLER = REPO_ROOT / "tools" / "luna_installer.py"
OVMF_RUN_VARS = ROOT / "ovmf-installercheck-vars.fd"


def find_qemu() -> str:
    found = shutil.which("qemu-system-x86_64")
    if found:
        return found
    for candidate in (
        r"C:\Program Files\qemu\qemu-system-x86_64.exe",
        r"C:\Program Files\QEMU\qemu-system-x86_64.exe",
    ):
        if Path(candidate).exists():
            return candidate
    raise FileNotFoundError("qemu-system-x86_64 not found")


def find_ovmf_code() -> str:
    env = os.environ.get("LUNA_OVMF_CODE")
    if env and Path(env).exists():
        return env
    for candidate in (
        r"C:\Program Files\qemu\share\edk2-x86_64-code.fd",
        r"C:\Program Files\QEMU\share\edk2-x86_64-code.fd",
    ):
        if Path(candidate).exists():
            return candidate
    raise FileNotFoundError("OVMF code firmware not found")


def find_ovmf_vars() -> str:
    env = os.environ.get("LUNA_OVMF_VARS")
    if env and Path(env).exists():
        return env
    for candidate in (
        r"C:\Program Files\qemu\share\edk2-i386-vars.fd",
        r"C:\Program Files\QEMU\share\edk2-i386-vars.fd",
        str(ROOT / "ovmf-vars.fd"),
    ):
        if Path(candidate).exists():
            return candidate
    raise FileNotFoundError("OVMF vars template not found")


def run_installer() -> None:
    if TARGET_IMAGE.exists():
        TARGET_IMAGE.unlink()
    subprocess.run(
        [
            sys.executable,
            str(INSTALLER),
            "--source",
            str(SOURCE_IMAGE),
            "apply",
            "--target-image",
            str(TARGET_IMAGE),
            "--yes",
        ],
        cwd=REPO_ROOT,
        check=True,
    )
    subprocess.run(
        [
            sys.executable,
            str(INSTALLER),
            "--source",
            str(SOURCE_IMAGE),
            "verify",
            "--target-image",
            str(TARGET_IMAGE),
        ],
        cwd=REPO_ROOT,
        check=True,
    )


def main() -> int:
    qemu = find_qemu()
    firmware = find_ovmf_code()
    vars_template = find_ovmf_vars()
    run_installer()
    if LOG_PATH.exists():
        LOG_PATH.unlink()
    if ERR_PATH.exists():
        ERR_PATH.unlink()
    shutil.copyfile(vars_template, OVMF_RUN_VARS)

    with LOG_PATH.open("w", encoding="utf-8", errors="replace") as stdout_file, ERR_PATH.open("w", encoding="utf-8", errors="replace") as stderr_file:
        proc = subprocess.Popen(
            [
                qemu,
                "-machine",
                "q35",
                "-device",
                "virtio-keyboard-pci",
                "-drive",
                f"if=pflash,format=raw,readonly=on,file={firmware}",
                "-drive",
                f"if=pflash,format=raw,file={OVMF_RUN_VARS}",
                "-drive",
                f"format=raw,file={TARGET_IMAGE}",
                "-serial",
                "stdio",
                "-display",
                "none",
                "-no-reboot",
                "-no-shutdown",
            ],
            stdout=stdout_file,
            stderr=stderr_file,
            text=True,
            encoding="utf-8",
            errors="replace",
        )

        try:
            deadline = time.time() + 45.0
            while time.time() < deadline:
                if LOG_PATH.exists():
                    text = LOG_PATH.read_text(encoding="utf-8", errors="replace")
                    if "[USER] shell ready" in text and "first-setup required: no hostname or user configured" in text:
                        break
                if proc.poll() is not None:
                    break
                time.sleep(0.25)
            if proc.poll() is None:
                proc.kill()
            proc.wait(timeout=5)
        finally:
            if proc.poll() is None:
                proc.kill()

    stdout = LOG_PATH.read_text(encoding="utf-8", errors="replace")
    required = [
        "[BOOT] dawn online",
        "[GRAPHICS] console ready",
        "[USER] shell ready",
        "first-setup required: no hostname or user configured",
    ]
    for needle in required:
        if needle not in stdout:
            raise RuntimeError(f"installercheck missing expected output: {needle}")

    sys.stdout.write(stdout)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
