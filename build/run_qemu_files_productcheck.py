from __future__ import annotations

import re
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent
IMAGE = ROOT / "lunaos.img"
FILESCHECK_IMAGE = ROOT / "lunaos_files_productcheck.img"
LOG_PATH = ROOT / "qemu_files_productcheck.log"
ERR_PATH = ROOT / "qemu_files_productcheck.err.log"
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
    FILESCHECK_IMAGE.write_bytes(blob)


def require(text: str, needles: list[str]) -> None:
    for needle in needles:
        if needle not in text:
            raise RuntimeError(f"files_productcheck missing expected output: {needle}")


def validate_files_metadata_consistency(text: str) -> None:
    entry = re.search(
        r"files\.entry 1 name=(?P<name>\S+) kind=(?P<kind>\S+) "
        r"type=(?P<type>[0-9A-F]+) state=(?P<state>\S+) "
        r"source=lafs query=lasql ref=0x(?P<ref>[0-9A-F]+)",
        text,
    )
    if not entry:
        raise RuntimeError("files_productcheck missing parseable LaFS entry row")

    meta_head = re.search(
        r"files\.meta selected=0 name=(?P<name>\S+) kind=(?P<kind>\S+) "
        r"source=LaFS owner-space=DATA consumer=USER query=LaSQL",
        text,
    )
    if not meta_head:
        raise RuntimeError("files_productcheck missing parseable selected metadata header")

    meta_ref = re.search(
        r"files\.meta ref=0x(?P<ref>[0-9A-F]+) type=(?P<type>[0-9A-F]+) "
        r"state=(?P<state>\S+) flags=(?P<flags>[0-9A-F]+) owner-ref=0x(?P<owner>[0-9A-F]+)",
        text,
    )
    if not meta_ref:
        raise RuntimeError("files_productcheck missing parseable selected metadata ref row")

    status = re.search(
        r"files\.status selected=0 state=(?P<state>\S+) normal=(?P<normal>yes|no) "
        r"readonly=(?P<readonly>yes|no) recovery=(?P<recovery>yes|no) "
        r"denied=(?P<denied>yes|no) missing=(?P<missing>yes|no)",
        text,
    )
    if not status:
        raise RuntimeError("files_productcheck missing parseable status row")

    for key in ("name", "kind"):
        if entry.group(key) != meta_head.group(key):
            raise RuntimeError(f"files_productcheck entry/meta mismatch for {key}")
    for key in ("ref", "type", "state"):
        if entry.group(key) != meta_ref.group(key):
            raise RuntimeError(f"files_productcheck entry/meta mismatch for {key}")
    if meta_ref.group("state") != status.group("state"):
        raise RuntimeError("files_productcheck metadata/status state mismatch")
    if entry.group("name") == "object" or int(entry.group("ref"), 16) == 0:
        raise RuntimeError("files_productcheck entry row does not look like a real LaFS object")
    if meta_ref.group("state") == "normal" and status.group("normal") != "yes":
        raise RuntimeError("files_productcheck normal state is not surfaced as normal=yes")
    if meta_ref.group("state") != "normal" and status.group("normal") == "yes":
        raise RuntimeError("files_productcheck non-normal state is incorrectly surfaced as normal=yes")


def main() -> int:
    qemu = find_qemu()
    commands = (
        b"setup.init luna dev secret\r\n"
        b"home.status\r\n"
        b"space-map\r\n"
        b"store-info\r\n"
        b"run Files\r\n"
        b"desktop.boot\r\n"
        b"desktop.launch Files\r\n"
        b"desktop.window\r\n"
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
                f"format=raw,file={FILESCHECK_IMAGE}",
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
            deadline = time.time() + 75.0
            while time.time() < deadline:
                if LOG_PATH.exists():
                    text = LOG_PATH.read_text(encoding="utf-8", errors="replace")
                    if (
                        "files.status selected=0 state=" in text
                        and "desktop.window" in text
                        and "title=Files" in text
                        and "dev@luna:~$" in text
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
    stderr = ERR_PATH.read_text(encoding="utf-8", errors="replace") if ERR_PATH.exists() else ""
    if "Exception Type" in stdout or "Can't find image information" in stdout:
        raise RuntimeError("files_productcheck saw firmware or loader exception")
    if stderr.strip():
        raise RuntimeError(f"files_productcheck qemu stderr not empty: {stderr.strip()[:240]}")

    require(
        stdout,
        [
            "[USER] shell ready",
            "setup.init ok: host and first user created",
            "DATA state=active mem=8 time=0 event=LAFSDISK",
            "lafs.version: 3",
            "lafs.objects:",
            "run Files",
            "files surface ready",
            "files.source=lafs target=user-documents query=lasql owner=DATA consumer=USER",
            "files.entry 1 name=",
            "source=lafs query=lasql ref=0x",
            "files.meta selected=0 name=",
            "source=LaFS owner-space=DATA consumer=USER query=LaSQL",
            "files.meta ref=0x",
            "files.meta created=",
            "size=unavailable record-count=unavailable",
            "files.status selected=0 state=",
            "desktop.launch Files",
            "[DESKTOP] launch Files ok",
            "title=Files",
        ],
    )
    validate_files_metadata_consistency(stdout)
    sys.stdout.write(stdout)
    sys.stdout.write("\nfiles_productcheck mode=scripted-product-evidence keyboard-navigation=desktopinputcheck\n")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
