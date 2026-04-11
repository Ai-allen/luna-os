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
SHELLCHECK_DISK = ROOT / "lunaos_uefi_shellcheck.img"
LOG_PATH = ROOT / "qemu_uefi_shellcheck.log"
ERR_PATH = ROOT / "qemu_uefi_shellcheck.err.log"
VARS_PATH = ROOT / "ovmf-uefi-shellcheck-vars.fd"

ESP_START_LBA = 2048
ESP_RESERVED_SECTORS = 1
ESP_FAT_COUNT = 2
ESP_FAT_SECTORS = 64
ESP_ROOT_ENTRIES = 512
DISK_SECTOR_SIZE = 512
SESSION_MAGIC = 0x54504353
DEV_BUNDLE_STAGE_MAGIC = 0x4244564C
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


def patch_session_script(disk_path: Path, commands: bytes, staged_bundle: bytes = b"") -> None:
    image_base, script_base, script_size = parse_session_layout()
    payload_capacity = script_size - 8
    extra = b""
    if staged_bundle:
        extra = struct.pack("<II", DEV_BUNDLE_STAGE_MAGIC, len(staged_bundle)) + staged_bundle
    if len(commands) + len(extra) > payload_capacity:
        raise RuntimeError("session script exceeds configured capacity")

    blob = bytearray(disk_path.read_bytes())
    stage_lba = ESP_START_LBA + ESP_DATA_START_LBA + (ESP_STAGE_FILE_CLUSTER - 2)
    offset = stage_lba * DISK_SECTOR_SIZE + (script_base - image_base)
    header = struct.pack("<II", SESSION_MAGIC, len(commands))
    payload = header + commands + extra + (b"\x00" * (script_size - len(header) - len(commands) - len(extra)))
    blob[offset:offset + script_size] = payload
    disk_path.write_bytes(blob)


def pack_devloop_bundle() -> bytes:
    repo_root = ROOT.parent
    output = ROOT / "uefi_devloop_sample.luna"
    subprocess.run(
        [
            sys.executable,
            str(repo_root / "tools" / "luna_pack.py"),
            str(repo_root / "tools" / "luna_manifest.minimal.json"),
            str(repo_root / "build" / "obj" / "console_app.bin"),
            str(output),
        ],
        cwd=repo_root,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return output.read_bytes()


def strip_ansi(text: str) -> str:
    return re.sub(r"\x1B\[[0-9;?=]*[ -/]*[@-~]", "", text)


def main() -> int:
    qemu = find_qemu()
    code = find_ovmf_code()
    vars_template = find_ovmf_vars_template()
    shutil.copyfile(DISK, SHELLCHECK_DISK)
    shutil.copyfile(vars_template, VARS_PATH)

    commands = (
        b"setup.status\r\n"
        b"setup.init luna dev secret\r\n"
        b"whoami\r\n"
        b"hostname\r\n"
        b"home.status\r\n"
        b"cap-count\r\n"
        b"space-map\r\n"
        b"list-apps\r\n"
        b"package.install sample\r\n"
        b"list-apps\r\n"
        b"run sample\r\n"
        b"package.remove sample\r\n"
        b"list-apps\r\n"
        b"run sample\r\n"
        b"run Settings\r\n"
        b"settings.status\r\n"
        b"net.loop\r\n"
        b"net.info\r\n"
        b"net.external\r\n"
        b"run Files\r\n"
    )
    patch_session_script(SHELLCHECK_DISK, commands, pack_devloop_bundle())

    for path in (LOG_PATH, ERR_PATH):
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
                f"format=raw,file={SHELLCHECK_DISK}",
                "-device",
                "virtio-keyboard-pci",
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
                if LOG_PATH.exists():
                    text = strip_ansi(LOG_PATH.read_text(encoding="utf-8", errors="replace"))
                    if "run Files" in text and "dev@luna:~$" in text:
                        break
                    if "Exception Type" in text or "Can't find image information" in text:
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
    required = [
        "LunaLoader UEFI Stage 1 handoff",
        "[BOOT] dawn online",
        "[SYSTEM] Space 14 ready.",
        "[USER] shell ready",
        "[USER] input lane ready",
        "first-setup required: no hostname or user configured",
        "setup.status",
        "setup state=required login=locked user=guest host=luna",
        "setup.next=setup.init",
        "setup.init luna dev secret",
        "setup.init ok: host and first user created",
        "login ok: session active",
        "whoami",
        "dev",
        "hostname",
        "luna",
        "home.status",
        "home owner=dev@luna",
        "home.root=0x4C414653",
        "cap-count",
        "caps: 021",
        "space-map",
        "package.install sample",
        "package.install ok",
        "package.install app=sample.luna status=installed",
        "run sample",
        "launch request: sample.luna",
        "package.remove sample",
        "package.remove ok",
        "package.remove app=sample status=removed",
        "run Settings",
        "settings surface ready",
        "settings.status",
        "settings.update state=idle current=0 target=0 user=dev",
        "settings.update entry=update.status action=check-only",
        "settings.pairing state=unavailable peer=0 user=dev",
        "settings.pairing entry=pairing.status action=trust loop peer",
        "net.loop",
        "net.loop state=ready scope=local-only",
        "net.info",
        "network.entry inspect=net.info verify=net.external diagnostics=net.loop",
        "net.info driver=13 flags=19 vendor=8086 device=10D3",
        "net.external",
        "net.external state=ready scope=outbound-only",
        "net.external bytes=8 data=luna-ext",
        "run Files",
        "files surface ready",
        "dev@luna:~$",
    ]
    for needle in required:
        if needle not in stdout:
            raise RuntimeError(f"uefi shellcheck missing expected output: {needle}")
    if "setup.init fail: setup not applied" in stdout:
        raise RuntimeError("uefi shellcheck should no longer fail first-setup apply under UEFI")
    if "Exception Type" in stdout or "Can't find image information" in stdout:
        raise RuntimeError("uefi shellcheck encountered a firmware exception dump")

    sys.stdout.write(stdout)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
