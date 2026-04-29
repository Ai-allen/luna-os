from __future__ import annotations

import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent
IMAGE = ROOT / "lunaos.img"
LSYS_CONTRACT_IMAGE = ROOT / "lunaos_lsys_contractfail.img"
LSYS_CONTRACT_LOG = ROOT / "qemu_lsys_contractfail.log"
LSYS_CONTRACT_ERR = ROOT / "qemu_lsys_contractfail.err.log"
ACTIVATION_RECOVERY_IMAGE = ROOT / "lunaos_activation_recovery.img"
ACTIVATION_RECOVERY_LOG = ROOT / "qemu_activation_recovery.log"
ACTIVATION_RECOVERY_ERR = ROOT / "qemu_activation_recovery.err.log"
SYSTEM_STORE_LBA = 0x4800
SUPERBLOCK_RESERVED_OFFSET = 56
SUPERBLOCK_ACTIVATION_OFFSET = SUPERBLOCK_RESERVED_OFFSET + (9 * 8)
SUPERBLOCK_PEER_LBA_OFFSET = SUPERBLOCK_RESERVED_OFFSET + (10 * 8)
LUNA_ACTIVATION_RECOVERY = 4
POST_GATE_OBSERVE_SECONDS = 3.0


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


def patch_u64(image_path: Path, offset: int, value: int) -> None:
    blob = bytearray(image_path.read_bytes())
    struct.pack_into("<Q", blob, offset, value)
    image_path.write_bytes(blob)


def patch_lsys_peer_contract(image_path: Path) -> None:
    offset = SYSTEM_STORE_LBA * 512 + SUPERBLOCK_PEER_LBA_OFFSET
    patch_u64(image_path, offset, 0)


def patch_activation_recovery(image_path: Path) -> None:
    offset = SYSTEM_STORE_LBA * 512 + SUPERBLOCK_ACTIVATION_OFFSET
    patch_u64(image_path, offset, LUNA_ACTIVATION_RECOVERY)


def run_qemu_once(
    qemu: str,
    image_path: Path,
    log_path: Path,
    err_path: Path,
    stop_markers: tuple[str, ...],
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
            gate_seen_at = None
            while time.time() < deadline:
                text = log_path.read_text(encoding="utf-8", errors="replace") if log_path.exists() else ""
                if "[USER] shell ready" in text:
                    break
                if any(marker in text for marker in stop_markers):
                    if gate_seen_at is None:
                        gate_seen_at = time.time()
                    elif time.time() - gate_seen_at >= POST_GATE_OBSERVE_SECONDS:
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


def require_absent(text: str, needle: str, label: str) -> None:
    if needle in text:
        raise RuntimeError(f"{label} observed forbidden output: {needle}")


def main() -> int:
    qemu = find_qemu()

    shutil.copyfile(IMAGE, LSYS_CONTRACT_IMAGE)
    patch_lsys_peer_contract(LSYS_CONTRACT_IMAGE)
    contract_text = run_qemu_once(
        qemu,
        LSYS_CONTRACT_IMAGE,
        LSYS_CONTRACT_LOG,
        LSYS_CONTRACT_ERR,
        ("[SYSTEM] storage gate=fatal", "[USER] shell ready"),
    )
    for needle in (
        "[BOOT] lsys super read ok",
        "[BOOT] lsys contract fail",
        "[DEVICE] disk path driver=piix-ide family=0000000C chain=ahci>fwblk>ata mode=fatal",
        "[SYSTEM] storage gate=fatal",
    ):
        require(contract_text, needle, "lsys-contract")
    for needle in (
        "[SYSTEM] storage gate=recovery",
        "[SYSTEM] driver gate=recovery",
        "[SYSTEM] Spawning PACKAGE space...",
        "[SYSTEM] Spawning UPDATE space...",
        "[SYSTEM] Spawning USER space...",
        "[UPDATE] wave ready",
        "[USER] shell ready",
    ):
        require_absent(contract_text, needle, "lsys-contract")

    shutil.copyfile(IMAGE, ACTIVATION_RECOVERY_IMAGE)
    patch_activation_recovery(ACTIVATION_RECOVERY_IMAGE)
    activation_text = run_qemu_once(
        qemu,
        ACTIVATION_RECOVERY_IMAGE,
        ACTIVATION_RECOVERY_LOG,
        ACTIVATION_RECOVERY_ERR,
        ("[SYSTEM] storage gate=recovery", "[USER] shell ready"),
    )
    for needle in (
        "[BOOT] lsys super read ok",
        "[BOOT] native pair ok",
        "[BOOT] activation recovery",
        "[DEVICE] disk path driver=piix-ide family=0000000C chain=ahci>fwblk>ata mode=recovery",
        "[SYSTEM] storage gate=recovery",
    ):
        require(activation_text, needle, "activation-recovery")
    for needle in (
        "[SYSTEM] storage gate=fatal",
        "[SYSTEM] driver gate=recovery",
        "[SYSTEM] Spawning PACKAGE space...",
        "[SYSTEM] Spawning UPDATE space...",
        "[SYSTEM] Spawning USER space...",
        "[UPDATE] wave ready",
        "[USER] shell ready",
    ):
        require_absent(activation_text, needle, "activation-recovery")

    sys.stdout.write(contract_text)
    sys.stdout.write("\n=== activation recovery ===\n")
    sys.stdout.write(activation_text)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
