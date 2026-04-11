from __future__ import annotations

import re
import shutil
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent
DISK = ROOT / "lunaos_disk.img"
LOG_PATH = ROOT / "qemu_uefi_bootcheck.log"
ERR_PATH = ROOT / "qemu_uefi_bootcheck.err.log"
DEBUG_PATH = ROOT / "qemu_uefi_bootcheck.debug.log"
VARS_PATH = ROOT / "ovmf-uefi-bootcheck-vars.fd"


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


def strip_ansi(text: str) -> str:
    return re.sub(r"\x1B\[[0-9;?=]*[ -/]*[@-~]", "", text)


def main() -> int:
    qemu = find_qemu()
    code = find_ovmf_code()
    vars_template = find_ovmf_vars_template()
    shutil.copyfile(vars_template, VARS_PATH)

    for path in (LOG_PATH, ERR_PATH, DEBUG_PATH):
        if path.exists():
            path.unlink()

    with LOG_PATH.open("w", encoding="utf-8", errors="replace") as stdout_file, ERR_PATH.open(
        "w", encoding="utf-8", errors="replace"
    ) as stderr_file:
        proc = subprocess.Popen(
            [
                qemu,
                "-machine",
                "q35",
                "-drive",
                f"if=pflash,format=raw,readonly=on,file={code}",
                "-drive",
                f"if=pflash,format=raw,file={VARS_PATH}",
                "-drive",
                f"format=raw,file={DISK}",
                "-serial",
                "stdio",
                "-debugcon",
                f"file:{DEBUG_PATH}",
                "-global",
                "isa-debugcon.iobase=0x402",
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
            deadline = time.time() + 60.0
            matched = False
            matched_at = 0.0
            while time.time() < deadline:
                if LOG_PATH.exists():
                    text = strip_ansi(LOG_PATH.read_text(encoding="utf-8", errors="replace"))
                    if "Exception Type" in text or "Can't find image information" in text:
                        break
                    if (
                        "LunaLoader UEFI Stage 1 handoff" in text
                        and "[BOOT] dawn online" in text
                        and "[SYSTEM] Space 14 ready." in text
                        and "[USER] shell ready" in text
                    ):
                        if not matched:
                            matched = True
                            matched_at = time.time()
                        elif time.time() - matched_at >= 2.0:
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

    stdout = strip_ansi(LOG_PATH.read_text(encoding="utf-8", errors="replace"))
    if "LunaLoader UEFI Stage 1 handoff" not in stdout:
        raise RuntimeError("UEFI bootcheck did not reach BOOTX64.EFI handoff output before timeout")
    if "[BOOT] dawn online" not in stdout:
        raise RuntimeError("UEFI bootcheck did not hand off into LunaOS BOOT runtime before timeout")
    if "[SYSTEM] Space 14 ready." not in stdout:
        raise RuntimeError("UEFI bootcheck did not reach the full space bring-up before timeout")
    if "[USER] shell ready" not in stdout:
        raise RuntimeError("UEFI bootcheck did not reach the Luna shell before timeout")
    if "Exception Type" in stdout or "Can't find image information" in stdout:
        raise RuntimeError("UEFI bootcheck encountered a firmware exception dump")

    sys.stdout.write(stdout)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
