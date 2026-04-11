from __future__ import annotations

import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent
IMAGE = ROOT / "lunaos.img"
REPAIR_IMAGE = ROOT / "lunaos_metarepair.img"
BOOT1_LOG = ROOT / "qemu_metarepair_boot1.log"
BOOT1_ERR = ROOT / "qemu_metarepair_boot1.err.log"
BOOT2_LOG = ROOT / "qemu_metarepair_boot2.log"
BOOT2_ERR = ROOT / "qemu_metarepair_boot2.err.log"
BOOT_SECTOR_BYTES = 512
SESSION_MAGIC = 0x54504353
STORE_BASE_LBA = 18432
SUPERBLOCK_CHECKSUM_OFFSET = 64
BROKEN_CHECKSUM = 0


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


def patch_session_script(image_path: Path, commands: bytes) -> None:
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

    blob = bytearray(image_path.read_bytes())
    offset = stage_offset + (script_base - image_base)
    header = struct.pack("<II", SESSION_MAGIC, len(commands))
    payload = header + commands + (b"\x00" * (script_size - len(header) - len(commands)))
    blob[offset:offset + script_size] = payload
    image_path.write_bytes(blob)


def patch_bad_checksum(image_path: Path) -> None:
    blob = bytearray(image_path.read_bytes())
    offset = STORE_BASE_LBA * 512 + SUPERBLOCK_CHECKSUM_OFFSET
    blob[offset:offset + 8] = struct.pack("<Q", BROKEN_CHECKSUM)
    image_path.write_bytes(blob)


def run_qemu_once(qemu: str, image_path: Path, log_path: Path, err_path: Path, wait_for: str) -> str:
    if log_path.exists():
        log_path.unlink()
    if err_path.exists():
        err_path.unlink()

    with log_path.open("w", encoding="utf-8", errors="replace") as stdout_file, err_path.open(
        "w", encoding="utf-8", errors="replace"
    ) as stderr_file:
        proc = subprocess.Popen(
            [
                qemu,
                "-drive",
                f"format=raw,file={image_path}",
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
                if log_path.exists():
                    text = log_path.read_text(encoding="utf-8", errors="replace")
                    if wait_for in text:
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

    return log_path.read_text(encoding="utf-8", errors="replace")


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise RuntimeError(f"{label} missing expected output: {needle}")


def main() -> int:
    qemu = find_qemu()
    shutil.copyfile(IMAGE, REPAIR_IMAGE)

    boot1_commands = (
        b"setup.init luna dev secret\r\n"
        b"run Notes\r\n"
    )
    patch_session_script(REPAIR_IMAGE, boot1_commands)
    boot1 = run_qemu_once(qemu, REPAIR_IMAGE, BOOT1_LOG, BOOT1_ERR, "readback ok")
    require(boot1, "first-setup required: no hostname or user configured", "boot1")
    require(boot1, "setup.init luna dev secret", "boot1")
    require(boot1, "setup.init ok: host and first user created", "boot1")
    require(boot1, "login ok: session active", "boot1")
    require(boot1, "run Notes", "boot1")
    require(boot1, "launch request: Notes", "boot1")
    require(boot1, "workspace created", "boot1")
    require(boot1, "title firstlight", "boot1")
    require(boot1, "text Luna note seed", "boot1")

    patch_bad_checksum(REPAIR_IMAGE)

    boot2_commands = (
        b"login dev secret\r\n"
        b"store-info\r\n"
        b"store-check\r\n"
        b"run Files\r\n"
    )
    patch_session_script(REPAIR_IMAGE, boot2_commands)
    boot2 = run_qemu_once(qemu, REPAIR_IMAGE, BOOT2_LOG, BOOT2_ERR, "browser surface online")
    if "[DATA] format store" in boot2:
        raise RuntimeError("boot2 unexpectedly reformatted the data store")
    require(boot2, "login ok: session active", "boot2")
    require(boot2, "store-info", "boot2")
    require(boot2, "lafs.objects: 41", "boot2")
    require(boot2, "lafs.state: clean", "boot2")
    require(boot2, "lafs.mounts: 2", "boot2")
    require(boot2, "lafs.formats: 1", "boot2")
    require(boot2, "lafs.last-repair: metadata", "boot2")
    require(boot2, "lafs.last-scrubbed: 0", "boot2")
    require(boot2, "store-check", "boot2")
    require(boot2, "lafs.health: ok", "boot2")
    require(boot2, "lafs.invalid: 0", "boot2")
    require(boot2, "run Files", "boot2")
    require(boot2, "launch request: Files", "boot2")
    require(boot2, "files surface ready", "boot2")
    require(boot2, "files.user dev@luna", "boot2")
    require(boot2, "files.visible=2 selected=0", "boot2")

    sys.stdout.write(boot1)
    sys.stdout.write("\n=== checksum reboot ===\n")
    sys.stdout.write(boot2)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
