from __future__ import annotations

import hashlib
import shutil
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent
ISO = ROOT / "lunaos_installer.iso"
TARGET_IMAGE = ROOT / "qemu_installer_isolation_target.img"
LOG_PATH = ROOT / "qemu_installer_isolationcheck.log"
ERR_PATH = ROOT / "qemu_installer_isolationcheck.err.log"
VARS_PATH = ROOT / "ovmf-installer-isolation-vars.fd"
TARGET_SIZE = 128 * 1024 * 1024


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


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while True:
            chunk = handle.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def create_blank_target() -> None:
    with TARGET_IMAGE.open("wb") as handle:
        handle.seek(TARGET_SIZE - 1)
        handle.write(b"\0")


def main() -> int:
    qemu = find_qemu()
    ovmf_code = find_ovmf_code()
    vars_template = find_ovmf_vars_template()

    if TARGET_IMAGE.exists():
        TARGET_IMAGE.unlink()
    if LOG_PATH.exists():
        LOG_PATH.unlink()
    if ERR_PATH.exists():
        ERR_PATH.unlink()

    create_blank_target()
    baseline_hash = sha256_file(TARGET_IMAGE)
    shutil.copyfile(vars_template, VARS_PATH)

    with LOG_PATH.open("w", encoding="utf-8", errors="replace") as stdout_file, ERR_PATH.open("w", encoding="utf-8", errors="replace") as stderr_file:
        proc = subprocess.Popen(
            [
                qemu,
                "-machine",
                "q35",
                "-device",
                "virtio-keyboard-pci",
                "-drive",
                f"if=pflash,format=raw,readonly=on,file={ovmf_code}",
                "-drive",
                f"if=pflash,format=raw,file={VARS_PATH}",
                "-cdrom",
                str(ISO),
                "-drive",
                f"format=raw,file={TARGET_IMAGE}",
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
            deadline = time.time() + 45.0
            matched = False
            mutated_before_bind = False
            while time.time() < deadline:
                text = LOG_PATH.read_text(encoding="utf-8", errors="replace") if LOG_PATH.exists() else ""
                bind_seen = "[INSTALLER] target bind ok" in text
                current_hash = sha256_file(TARGET_IMAGE)
                if current_hash != baseline_hash and not bind_seen:
                    mutated_before_bind = True
                    break
                if "[BOOT] installer media mode" in text and bind_seen:
                    matched = True
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
        "[BOOT] installer media mode",
        "[INSTALLER] target bind ok",
    ]
    forbidden = [
        "audit package.install persisted=DATA authority=PACKAGE",
        "audit package.remove persisted=DATA authority=PACKAGE",
    ]
    for needle in required:
        if needle not in stdout:
            raise RuntimeError(f"installer isolation check missing expected output: {needle}")
    for needle in forbidden:
        if needle in stdout:
            raise RuntimeError(f"installer isolation check observed forbidden output: {needle}")
    if not matched:
        raise RuntimeError("installer isolation check did not reach expected installer bind state")
    if mutated_before_bind:
        raise RuntimeError("installer isolation check detected target disk mutation before installer binding")

    sys.stdout.write(stdout)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
