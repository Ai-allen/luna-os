from __future__ import annotations

import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent
IMAGE = ROOT / "lunaos.img"
SCRUB_IMAGE = ROOT / "lunaos_scrub.img"
BOOT1_LOG = ROOT / "qemu_scrub_boot1.log"
BOOT1_ERR = ROOT / "qemu_scrub_boot1.err.log"
BOOT2_LOG = ROOT / "qemu_scrub_boot2.log"
BOOT2_ERR = ROOT / "qemu_scrub_boot2.err.log"
BOOT_SECTOR_BYTES = 512
IMAGE_BASE = 0x10000
SESSION_MAGIC = 0x54504353
STORE_BASE_LBA = 1024
OBJECT_CAPACITY = 128
OBJECT_RECORD_SIZE = 88
SUPERBLOCK_STATE_OFFSET = 56
SUPERBLOCK_CHECKSUM_OFFSET = 64
STORE_STATE_DIRTY = 0x4449525459000001
BROKEN_SLOT_INDEX = 999


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


def parse_session_layout() -> tuple[int, int]:
    for line in (ROOT / "luna.ld").read_text(encoding="utf-8").splitlines():
        parts = line.strip().split()
        if len(parts) == 4 and parts[0] == "SCRIPT" and parts[1] == "SESSION":
            return int(parts[2], 16), int(parts[3], 16)
    raise RuntimeError("missing SCRIPT SESSION layout entry")


def patch_session_script(image_path: Path, commands: bytes) -> None:
    script_base, script_size = parse_session_layout()
    payload_capacity = script_size - 8
    if len(commands) > payload_capacity:
        raise RuntimeError("session script exceeds configured capacity")

    blob = bytearray(image_path.read_bytes())
    offset = BOOT_SECTOR_BYTES + (script_base - IMAGE_BASE)
    header = struct.pack("<II", SESSION_MAGIC, len(commands))
    payload = header + commands + (b"\x00" * (script_size - len(header) - len(commands)))
    blob[offset:offset + script_size] = payload
    image_path.write_bytes(blob)


def fold_bytes(seed: int, data: bytes) -> int:
    for value in data:
        seed ^= value
        seed = ((seed << 5) | (seed >> (64 - 5))) & 0xFFFFFFFFFFFFFFFF
        seed = (seed * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return seed


def object_checksum(table: bytes) -> int:
    return fold_bytes(0xCBF29CE484222325, table)


def patch_invalid_record_dirty(image_path: Path) -> None:
    blob = bytearray(image_path.read_bytes())
    super_offset = STORE_BASE_LBA * 512
    records_offset = (STORE_BASE_LBA + 1) * 512
    table_size = OBJECT_RECORD_SIZE * OBJECT_CAPACITY

    struct.pack_into("<I", blob, records_offset + 12, BROKEN_SLOT_INDEX)
    table = bytes(blob[records_offset:records_offset + table_size])
    struct.pack_into("<Q", blob, super_offset + SUPERBLOCK_STATE_OFFSET, STORE_STATE_DIRTY)
    struct.pack_into("<Q", blob, super_offset + SUPERBLOCK_CHECKSUM_OFFSET, object_checksum(table))
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
    shutil.copyfile(IMAGE, SCRUB_IMAGE)

    boot1_commands = b"run Notes\r\n"
    patch_session_script(SCRUB_IMAGE, boot1_commands)
    boot1 = run_qemu_once(qemu, SCRUB_IMAGE, BOOT1_LOG, BOOT1_ERR, "readback ok")
    require(boot1, "[DATA] format store", "boot1")
    require(boot1, "run Notes", "boot1")
    require(boot1, "launch request: Notes", "boot1")
    require(boot1, "readback ok", "boot1")

    patch_invalid_record_dirty(SCRUB_IMAGE)

    boot2_commands = b"store-info\r\nstore-check\r\nrun Files\r\n"
    patch_session_script(SCRUB_IMAGE, boot2_commands)
    boot2 = run_qemu_once(qemu, SCRUB_IMAGE, BOOT2_LOG, BOOT2_ERR, "browser surface online")
    require(boot2, "[DATA] recovery replay", "boot2")
    require(boot2, "[DATA] scrub objects", "boot2")
    require(boot2, "store-info", "boot2")
    require(boot2, "lafs.objects: 3", "boot2")
    require(boot2, "lafs.state: clean", "boot2")
    require(boot2, "lafs.last-repair: scrub", "boot2")
    require(boot2, "lafs.last-scrubbed: 1", "boot2")
    require(boot2, "store-check", "boot2")
    require(boot2, "lafs.health: ok", "boot2")
    require(boot2, "lafs.invalid: 0", "boot2")
    require(boot2, "run Files", "boot2")
    require(boot2, "launch request: Files", "boot2")
    require(boot2, "visible objects: 2", "boot2")
    require(boot2, "browser surface online", "boot2")

    sys.stdout.write(boot1)
    sys.stdout.write("\n=== scrub reboot ===\n")
    sys.stdout.write(boot2)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
