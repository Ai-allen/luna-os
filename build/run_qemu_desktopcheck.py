from __future__ import annotations

import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent
IMAGE = ROOT / "lunaos.img"
DESKTOPCHECK_IMAGE = ROOT / "lunaos_desktopcheck.img"
LOG_PATH = ROOT / "qemu_desktopcheck.log"
ERR_PATH = ROOT / "qemu_desktopcheck.err.log"
BOOT_SECTOR_BYTES = 512
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
    raise FileNotFoundError("qemu-system-x86_64 not found. Install QEMU or add it to PATH.")


def parse_session_layout() -> tuple[int, int, int]:
    image_base = None
    for line in (ROOT / "luna.ld").read_text(encoding="utf-8").splitlines():
        parts = line.strip().split()
        if len(parts) == 2 and parts[0] == "IMAGE_BASE":
            image_base = int(parts[1], 16)
        if len(parts) == 4 and parts[0] == "SCRIPT" and parts[1] == "SESSION":
            if image_base is None:
                raise RuntimeError("missing IMAGE_BASE entry")
            return image_base, int(parts[2], 16), int(parts[3], 16)
    raise RuntimeError("missing SCRIPT SESSION layout entry")


def patch_session_script(commands: bytes) -> None:
    image_base, script_base, script_size = parse_session_layout()
    payload_capacity = script_size - 8
    if len(commands) > payload_capacity:
        raise RuntimeError("session script exceeds configured capacity")

    loader_path = ROOT / "obj" / "lunaloader_stage1.bin"
    if not loader_path.exists():
        loader_path = ROOT / "build" / "obj" / "lunaloader_stage1.bin"
    if not loader_path.exists():
        raise RuntimeError("missing lunaloader_stage1.bin")
    loader_sectors = (loader_path.stat().st_size + BOOT_SECTOR_BYTES - 1) // BOOT_SECTOR_BYTES
    stage_offset = BOOT_SECTOR_BYTES + loader_sectors * BOOT_SECTOR_BYTES

    blob = bytearray(IMAGE.read_bytes())
    offset = stage_offset + (script_base - image_base)
    header = struct.pack("<II", SESSION_MAGIC, len(commands))
    payload = header + commands + (b"\x00" * (script_size - len(header) - len(commands)))
    blob[offset:offset + script_size] = payload
    DESKTOPCHECK_IMAGE.write_bytes(blob)


def main() -> int:
    qemu = find_qemu()
    commands = (
        b"setup.init luna dev secret\r\n"
        b"desktop.boot\r\n"
        b"desktop.menu\r\n"
        b"desktop.status\r\n"
        b"desktop.launch Settings\r\n"
        b"desktop.window\r\n"
        b"desktop.settings\r\n"
        b"desktop.status\r\n"
        b"desktop.launch Files\r\n"
        b"desktop.window\r\n"
        b"desktop.files.next\r\n"
        b"desktop.files.open\r\n"
        b"desktop.status\r\n"
        b"desktop.launch Notes\r\n"
        b"desktop.note workspace draft\r\n"
        b"desktop.window\r\n"
        b"desktop.status\r\n"
        b"desktop.launch Console\r\n"
        b"desktop.window\r\n"
        b"desktop.focus\r\n"
        b"desktop.window\r\n"
        b"desktop.window\r\n"
        b"desktop.minimize\r\n"
        b"desktop.window\r\n"
        b"desktop.focus\r\n"
        b"desktop.window\r\n"
        b"desktop.maximize\r\n"
        b"desktop.window\r\n"
        b"desktop.control\r\n"
        b"desktop.theme\r\n"
        b"desktop.status\r\n"
        b"desktop.close\r\n"
        b"desktop.window\r\n"
        b"desktop.status\r\n"
    )
    patch_session_script(commands)
    if LOG_PATH.exists():
        LOG_PATH.unlink()
    if ERR_PATH.exists():
        ERR_PATH.unlink()

    with LOG_PATH.open("w", encoding="utf-8", errors="replace") as stdout_file, ERR_PATH.open("w", encoding="utf-8", errors="replace") as stderr_file:
        proc = subprocess.Popen(
            [
                qemu,
                "-drive",
                f"format=raw,file={DESKTOPCHECK_IMAGE}",
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
            deadline = time.time() + 60.0
            while time.time() < deadline:
                if LOG_PATH.exists():
                    text = LOG_PATH.read_text(encoding="utf-8", errors="replace")
                    if (
                        "setup.init ok: host and first user created" in text and
                        "desktop.launch Console" in text and
                        "[DESKTOP] launch Console ok" in text and
                        "desktop.close" in text and
                        "[DESKTOP] close ok" in text and
                        "dev@luna:~$" in text
                    ):
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
        "[USER] shell ready",
        "first-setup required: no hostname or user configured",
        "setup.init luna dev secret",
        "setup.init ok: host and first user created",
        "login ok: session active",
        "desktop.boot",
        "[DESKTOP] boot ok",
        "desktop.menu",
        "[DESKTOP] menu ok",
        "desktop.status",
        "desktop.launch Settings",
        "settings surface ready",
        "[DESKTOP] launch Settings ok",
        "desktop.window",
        "[DESKTOP] window id=",
        "title=Settings x=40 y=14 w=17 h=7 min=0 max=0",
        "desktop.settings",
        "[DESKTOP] settings ok",
        "desktop.launch Settings",
        "desktop.launch Files",
        "files surface ready",
        "[DESKTOP] launch Files ok",
        "title=Files x=2 y=2 w=26 h=9 min=0 max=0",
        "desktop.files.next",
        "[DESKTOP] files.next ok",
        "desktop.files.open",
        "[DESKTOP] files.open ok",
        "desktop.launch Notes",
        "workspace created",
        "[DESKTOP] launch Notes ok",
        "desktop.note workspace draft",
        "[DESKTOP] note ok",
        "title=Notes x=29 y=2 w=27 h=9 min=0 max=0",
        "desktop.launch Console",
        "apps 1-5 or F/N/G/C/H",
        "[DESKTOP] launch Console ok",
        "title=Console x=2 y=12 w=37 h=11 min=0 max=0",
        "desktop.focus",
        "[DESKTOP] focus ok",
        "desktop.minimize",
        "[DESKTOP] minimize ok",
        "desktop.maximize",
        "[DESKTOP] maximize ok",
        "desktop.control",
        "[DESKTOP] control ok",
        "desktop.theme",
        "[DESKTOP] theme ok",
        "desktop.close",
        "[DESKTOP] close ok",
        "dev@luna:~$",
    ]
    for needle in required:
        if needle not in stdout:
            raise RuntimeError(f"desktopcheck missing expected output: {needle}")

    sys.stdout.write(stdout)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
