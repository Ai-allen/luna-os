from __future__ import annotations

import shutil
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parent
SOURCE_DISK = ROOT / "lunaos_disk.img"
TARGET_DISK = ROOT / "lunaos_vmware_desktopcheck.img"

ESP_START_LBA = 2048
ESP_RESERVED_SECTORS = 1
ESP_FAT_COUNT = 2
ESP_FAT_SECTORS = 64
ESP_ROOT_ENTRIES = 512
DISK_SECTOR_SIZE = 512
SESSION_MAGIC = 0x54504353
ESP_ROOT_DIR_SECTORS = (ESP_ROOT_ENTRIES * 32 + DISK_SECTOR_SIZE - 1) // DISK_SECTOR_SIZE
ESP_DATA_START_LBA = ESP_RESERVED_SECTORS + ESP_FAT_COUNT * ESP_FAT_SECTORS + ESP_ROOT_DIR_SECTORS
ESP_STAGE_FILE_CLUSTER = 64


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


def patch_session_script(disk_path: Path, commands: bytes) -> None:
    image_base, script_base, script_size = parse_session_layout()
    payload_capacity = script_size - 8
    if len(commands) > payload_capacity:
        raise RuntimeError("session script exceeds configured capacity")

    blob = bytearray(disk_path.read_bytes())
    stage_lba = ESP_START_LBA + ESP_DATA_START_LBA + (ESP_STAGE_FILE_CLUSTER - 2)
    offset = stage_lba * DISK_SECTOR_SIZE + (script_base - image_base)
    header = struct.pack("<II", SESSION_MAGIC, len(commands))
    payload = header + commands + (b"\x00" * (script_size - len(header) - len(commands)))
    blob[offset:offset + script_size] = payload
    disk_path.write_bytes(blob)


def main() -> int:
    commands = (
        b"setup.init luna dev secret\r\n"
        b"login dev secret\r\n"
        b"desktop.boot\r\n"
        b"desktop.status\r\n"
        b"desktop.launch Settings\r\n"
        b"desktop.status\r\n"
        b"desktop.launch Files\r\n"
        b"desktop.status\r\n"
        b"desktop.launch Console\r\n"
        b"desktop.status\r\n"
    )
    shutil.copyfile(SOURCE_DISK, TARGET_DISK)
    patch_session_script(TARGET_DISK, commands)
    print(TARGET_DISK)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
