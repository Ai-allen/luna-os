from __future__ import annotations

import os
import shutil
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent
REPO_ROOT = ROOT.parent
ISO = ROOT / "lunaos_installer.iso"
SOURCE_IMAGE = ROOT / "lunaos_disk.img"
TARGET_IMAGE = ROOT / "qemu_installer_target_apply.img"
INSTALL_LOG = ROOT / "qemu_installer_applycheck.log"
INSTALL_ERR = ROOT / "qemu_installer_applycheck.err.log"
BOOT_LOG = ROOT / "qemu_installer_target_bootcheck.log"
BOOT_ERR = ROOT / "qemu_installer_target_bootcheck.err.log"
INSTALL_VARS = ROOT / "ovmf-installer-target-vars.fd"
BOOT_VARS = ROOT / "ovmf-installer-target-boot-vars.fd"
TARGET_SIZE = 128 * 1024 * 1024
TARGET_ZERO_CHUNK_SIZE = 1024 * 1024
INSTALL_TIMEOUT_SECONDS = 180.0
BOOT_TIMEOUT_SECONDS = 90.0


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


def create_blank_target() -> None:
    zero_chunk = b"\0" * TARGET_ZERO_CHUNK_SIZE
    remaining = TARGET_SIZE
    with TARGET_IMAGE.open("wb") as handle:
        while remaining > 0:
            chunk_size = min(remaining, TARGET_ZERO_CHUNK_SIZE)
            handle.write(zero_chunk[:chunk_size])
            remaining -= chunk_size
        handle.flush()
        os.fsync(handle.fileno())


def cleanup_logs(*paths: Path) -> None:
    for path in paths:
        if path.exists():
            path.unlink()


def run_qemu_capture(args: list[str], log_path: Path, err_path: Path, success_needles: list[str], failure_needles: list[str], timeout_seconds: float) -> str:
    with log_path.open("w", encoding="utf-8", errors="replace") as stdout_file, err_path.open("w", encoding="utf-8", errors="replace") as stderr_file:
        proc = subprocess.Popen(
            args,
            stdout=stdout_file,
            stderr=stderr_file,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        try:
            deadline = time.time() + timeout_seconds
            matched = False
            while time.time() < deadline:
                text = log_path.read_text(encoding="utf-8", errors="replace") if log_path.exists() else ""
                if any(needle in text for needle in failure_needles):
                    break
                if all(needle in text for needle in success_needles):
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

    text = log_path.read_text(encoding="utf-8", errors="replace")
    for needle in failure_needles:
        if needle in text:
            raise RuntimeError(f"installer apply check observed failure output: {needle}")
    if not all(needle in text for needle in success_needles):
        missing = next(needle for needle in success_needles if needle not in text)
        raise RuntimeError(f"installer apply check missing expected output: {missing}")
    return text


def verify_target_image() -> None:
    subprocess.run(
        [
            sys.executable,
            str(REPO_ROOT / "tools" / "luna_installer.py"),
            "--source",
            str(SOURCE_IMAGE),
            "verify",
            "--target-image",
            str(TARGET_IMAGE),
        ],
        cwd=REPO_ROOT,
        check=True,
    )


def main() -> int:
    qemu = find_qemu()
    ovmf_code = find_ovmf_code()
    vars_template = find_ovmf_vars_template()

    cleanup_logs(INSTALL_LOG, INSTALL_ERR, BOOT_LOG, BOOT_ERR, TARGET_IMAGE, INSTALL_VARS, BOOT_VARS)
    create_blank_target()
    shutil.copyfile(vars_template, INSTALL_VARS)
    shutil.copyfile(vars_template, BOOT_VARS)

    install_log = run_qemu_capture(
        [
            qemu,
            "-machine",
            "q35",
            "-device",
            "virtio-keyboard-pci",
            "-drive",
            f"if=pflash,format=raw,readonly=on,file={ovmf_code}",
            "-drive",
            f"if=pflash,format=raw,file={INSTALL_VARS}",
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
        INSTALL_LOG,
        INSTALL_ERR,
        [
            "[BOOT] installer media mode",
            "[INSTALLER] target bind ok",
            "[INSTALLER] writer identity ok",
            "[INSTALLER] esp write ok",
            "[INSTALLER] lsys write ok",
            "[INSTALLER] ldat write ok",
            "[INSTALLER] lrcv write ok",
            "[INSTALLER] lrcv verify ok",
            "[INSTALLER] verify ok",
        ],
        [
            "[SYSTEM] installer gate=blocked",
            "[INSTALLER] target read cap fail",
            "[INSTALLER] writer identity fail",
            "[INSTALLER] gpt header fail",
            "[INSTALLER] gpt backup header fail",
            "[INSTALLER] esp source read fail",
            "[INSTALLER] esp target write fail",
            "[INSTALLER] lsys super fail",
            "[INSTALLER] ldat super fail",
            "[INSTALLER] lrcv write fail",
            "[INSTALLER] verify lrcv read fail",
            "[INSTALLER] verify lrcv contract fail",
            "[INSTALLER] verify gpt read fail",
            "[INSTALLER] verify lsys read fail",
            "[INSTALLER] verify ldat read fail",
            "[INSTALLER] verify gpt header fail",
            "[INSTALLER] verify lsys contract fail",
            "[INSTALLER] verify ldat contract fail",
            "[INSTALLER] lsys meta fail",
            "[INSTALLER] ldat meta fail",
            "[INSTALLER] gpt entries fail",
            "[INSTALLER] gpt backup entries fail",
        ],
        INSTALL_TIMEOUT_SECONDS,
    )

    verify_target_image()

    boot_log = run_qemu_capture(
        [
            qemu,
            "-machine",
            "q35",
            "-device",
            "virtio-keyboard-pci",
            "-drive",
            f"if=pflash,format=raw,readonly=on,file={ovmf_code}",
            "-drive",
            f"if=pflash,format=raw,file={BOOT_VARS}",
            "-drive",
            f"format=raw,file={TARGET_IMAGE}",
            "-serial",
            "stdio",
            "-display",
            "none",
            "-no-reboot",
            "-no-shutdown",
        ],
        BOOT_LOG,
        BOOT_ERR,
        [
            "[BOOT] dawn online",
            "[BOOT] fwblk handoff ok",
            "[BOOT] lsys super read ok",
            "[BOOT] native pair ok",
            "[BOOT] continuing boot",
            "[USER] shell ready",
            "first-setup required: no hostname or user configured",
        ],
        [
            "Exception Type",
            "Can't find image information",
            "[SYSTEM] installer gate=blocked",
            "[FWBLK] handoff missing",
            "[BOOT] lsys read fail",
            "[BOOT] lsys contract fail",
        ],
        BOOT_TIMEOUT_SECONDS,
    )

    sys.stdout.write(install_log)
    sys.stdout.write("\n--- TARGET BOOT ---\n")
    sys.stdout.write(boot_log)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
