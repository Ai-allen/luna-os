from __future__ import annotations

import re
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent
DISK = ROOT / "lunaos_disk.img"
PERSIST_DISK = ROOT / "lunaos_uefi_persistence.img"
BOOT1_LOG = ROOT / "qemu_uefi_persist_boot1.log"
BOOT1_ERR = ROOT / "qemu_uefi_persist_boot1.err.log"
BOOT2_LOG = ROOT / "qemu_uefi_persist_boot2.log"
BOOT2_ERR = ROOT / "qemu_uefi_persist_boot2.err.log"
VARS_PATH = ROOT / "ovmf-uefi-persist-vars.fd"

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


def strip_ansi(text: str) -> str:
    return re.sub(r"\x1B\[[0-9;?=]*[ -/]*[@-~]", "", text)


def run_qemu_once(
    qemu: str,
    ovmf_code: str,
    vars_path: Path,
    disk_path: Path,
    log_path: Path,
    err_path: Path,
    wait_for: str,
) -> str:
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
                "-machine",
                "q35",
                "-drive",
                f"if=pflash,format=raw,readonly=on,file={ovmf_code}",
                "-drive",
                f"if=pflash,format=raw,file={vars_path}",
                "-drive",
                f"format=raw,file={disk_path}",
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
            while time.time() < deadline:
                if log_path.exists():
                    text = strip_ansi(log_path.read_text(encoding="utf-8", errors="replace"))
                    if wait_for in text or "Exception Type" in text:
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

    return strip_ansi(log_path.read_text(encoding="utf-8", errors="replace"))


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise RuntimeError(f"{label} missing expected output: {needle}")


def main() -> int:
    qemu = find_qemu()
    ovmf_code = find_ovmf_code()
    vars_template = find_ovmf_vars_template()
    shutil.copyfile(DISK, PERSIST_DISK)
    shutil.copyfile(vars_template, VARS_PATH)

    patch_session_script(PERSIST_DISK, b"run Notes\r\n")
    boot1 = run_qemu_once(qemu, ovmf_code, VARS_PATH, PERSIST_DISK, BOOT1_LOG, BOOT1_ERR, "readback ok")
    require(boot1, "[DATA] lafs disk online", "boot1")
    require(boot1, "run Notes", "boot1")
    require(boot1, "launch request: Notes", "boot1")
    require(boot1, "readback ok", "boot1")
    require(boot1, "title: workspace", "boot1")
    require(boot1, "body: Luna note seed", "boot1")

    patch_session_script(PERSIST_DISK, b"run Files\r\n")
    boot2 = run_qemu_once(qemu, ovmf_code, VARS_PATH, PERSIST_DISK, BOOT2_LOG, BOOT2_ERR, "browser surface online")
    if "[DATA] format store" in boot2:
        raise RuntimeError("boot2 unexpectedly reformatted the data store")
    require(boot2, "run Files", "boot2")
    require(boot2, "launch request: Files", "boot2")
    require(boot2, "visible objects: 2", "boot2")
    require(boot2, "item [note] workspace | Luna note seed", "boot2")
    require(boot2, "browser surface online", "boot2")

    sys.stdout.write(boot1)
    sys.stdout.write("\n=== reboot ===\n")
    sys.stdout.write(boot2)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
