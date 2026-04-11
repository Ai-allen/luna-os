from __future__ import annotations

import shutil
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent
ISO = ROOT / "lunaos_installer.iso"
LOG_PATH = ROOT / "qemu_uefi_iso_bootcheck.log"
ERR_PATH = ROOT / "qemu_uefi_iso_bootcheck.err.log"
VARS_PATH = ROOT / "ovmf-uefi-iso-vars.fd"


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
    raise FileNotFoundError("qemu-system-x86_64 not found. Install QEMU or add it to PATH.")


def find_ovmf_code() -> str:
    for candidate in (
        Path(r"C:\Program Files\qemu\share\edk2-x86_64-code.fd"),
        Path(r"C:\Program Files\QEMU\share\edk2-x86_64-code.fd"),
    ):
        if candidate.exists():
            return str(candidate)
    raise FileNotFoundError("edk2 x86_64 code firmware not found")


def find_ovmf_vars_template() -> Path:
    for candidate in (
        Path(r"C:\Program Files\qemu\share\edk2-i386-vars.fd"),
        Path(r"C:\Program Files\QEMU\share\edk2-i386-vars.fd"),
        ROOT / "ovmf-vars.fd",
    ):
        if candidate.exists():
            return candidate
    raise FileNotFoundError("OVMF vars template not found")


def main() -> int:
    qemu = find_qemu()
    ovmf_code = find_ovmf_code()
    vars_template = find_ovmf_vars_template()
    shutil.copyfile(vars_template, VARS_PATH)
    if LOG_PATH.exists():
        LOG_PATH.unlink()
    if ERR_PATH.exists():
        ERR_PATH.unlink()

    with LOG_PATH.open("w", encoding="utf-8", errors="replace") as stdout_file, ERR_PATH.open("w", encoding="utf-8", errors="replace") as stderr_file:
        proc = subprocess.Popen(
            [
                qemu,
                "-machine",
                "q35",
                "-drive",
                f"if=pflash,format=raw,readonly=on,file={ovmf_code}",
                "-drive",
                f"if=pflash,format=raw,file={VARS_PATH}",
                "-cdrom",
                str(ISO),
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
            deadline = time.time() + 30.0
            matched = False
            while time.time() < deadline:
                if LOG_PATH.exists():
                    text = LOG_PATH.read_text(encoding="utf-8", errors="replace")
                    if (
                        "[USER] shell ready" in text and
                        "[USER] desktop session online" in text
                    ):
                        matched = True
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
    if not matched:
        raise RuntimeError("uefi iso bootcheck did not reach USER desktop or shell session before timeout")
    sys.stdout.write(stdout)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
