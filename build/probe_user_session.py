from __future__ import annotations

import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent
IMAGE = ROOT / "lunaos.img"
PROBE_IMAGE = ROOT / "lunaos_probe.img"
LOG_PATH = ROOT / "qemu_probe_user.log"
ERR_PATH = ROOT / "qemu_probe_user.err.log"
BOOT_SECTOR_BYTES = 512
IMAGE_BASE = 0x10000
SESSION_MAGIC = 0x54504353


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


def parse_session_layout() -> tuple[int, int]:
    for line in (ROOT / "luna.ld").read_text(encoding="utf-8").splitlines():
        parts = line.strip().split()
        if len(parts) == 4 and parts[0] == "SCRIPT" and parts[1] == "SESSION":
            return int(parts[2], 16), int(parts[3], 16)
    raise RuntimeError("missing SCRIPT SESSION layout entry")


def patch_session_script(commands: bytes) -> None:
    script_base, script_size = parse_session_layout()
    blob = bytearray(IMAGE.read_bytes())
    offset = BOOT_SECTOR_BYTES + (script_base - IMAGE_BASE)
    header = struct.pack("<II", SESSION_MAGIC, len(commands))
    payload = header + commands + (b"\x00" * (script_size - len(header) - len(commands)))
    blob[offset:offset + script_size] = payload
    PROBE_IMAGE.write_bytes(blob)


def main() -> int:
    qemu = find_qemu()
    patch_session_script(b"help\r\nwhoami\r\nhostname\r\n")
    if LOG_PATH.exists():
        LOG_PATH.unlink()
    if ERR_PATH.exists():
        ERR_PATH.unlink()

    with LOG_PATH.open("w", encoding="utf-8", errors="replace") as stdout_file, ERR_PATH.open("w", encoding="utf-8", errors="replace") as stderr_file:
        proc = subprocess.Popen(
            [
                qemu,
                "-drive",
                f"format=raw,file={PROBE_IMAGE}",
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
            deadline = time.time() + 25.0
            while time.time() < deadline:
                if LOG_PATH.exists():
                    text = LOG_PATH.read_text(encoding="utf-8", errors="replace")
                    if "session script" in text or "operator" in text:
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

    sys.stdout.write(LOG_PATH.read_text(encoding="utf-8", errors="replace"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
