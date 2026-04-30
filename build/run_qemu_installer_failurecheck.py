from __future__ import annotations

import shutil
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent
REPO_ROOT = ROOT.parent
ISO = ROOT / "lunaos_installer.iso"
SOURCE_IMAGE = ROOT / "lunaos_disk.img"
TARGET_IMAGE = ROOT / "qemu_installer_target_failure.img"
FAILURE_LOG = ROOT / "qemu_installer_failurecheck.log"
FAILURE_ERR = ROOT / "qemu_installer_failurecheck.err.log"
SUCCESS_LOG = ROOT / "qemu_installer_failure_recovery.log"
SUCCESS_ERR = ROOT / "qemu_installer_failure_recovery.err.log"
IDEMPOTENT_LOG = ROOT / "qemu_installer_idempotency.log"
IDEMPOTENT_ERR = ROOT / "qemu_installer_idempotency.err.log"
BOOT_LOG = ROOT / "qemu_installer_failure_boot.log"
BOOT_ERR = ROOT / "qemu_installer_failure_boot.err.log"
FAILURE_VARS = ROOT / "ovmf-installer-failure-vars.fd"
SUCCESS_VARS = ROOT / "ovmf-installer-failure-recovery-vars.fd"
IDEMPOTENT_VARS = ROOT / "ovmf-installer-idempotency-vars.fd"
BOOT_VARS = ROOT / "ovmf-installer-failure-boot-vars.fd"
FAILURE_TARGET_SIZE = 48 * 1024 * 1024
RECOVERY_TARGET_SIZE = 128 * 1024 * 1024
INSTALLER_PRODUCT_SPACE_MARKERS = (
    "[SYSTEM] Spawning DATA space...",
    "[SYSTEM] Space 2 ready.",
    "[SYSTEM] Spawning PACKAGE space...",
    "[SYSTEM] Spawning UPDATE space...",
    "[SYSTEM] Spawning USER space...",
    "[UPDATE] wave ready",
    "[USER] shell ready",
    "first-setup required: no hostname or user configured",
)


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


def create_target(path: Path, size_bytes: int) -> None:
    with path.open("wb") as handle:
        handle.seek(size_bytes - 1)
        handle.write(b"\0")
    require_target_size(path, size_bytes)


def resize_target(path: Path, size_bytes: int) -> None:
    with path.open("r+b") as handle:
        handle.truncate(size_bytes)
    require_target_size(path, size_bytes)


def require_target_size(path: Path, size_bytes: int) -> None:
    actual = path.stat().st_size
    if actual != size_bytes:
        raise RuntimeError(f"{path.name} size mismatch: expected {size_bytes}, got {actual}")


def cleanup_paths(*paths: Path) -> None:
    for path in paths:
        if path.exists():
            path.unlink()

def run_qemu_capture(args: list[str], log_path: Path, err_path: Path, stop_markers: tuple[str, ...], timeout_seconds: float) -> str:
    with log_path.open("w", encoding="utf-8", errors="replace") as stdout_file, err_path.open(
        "w", encoding="utf-8", errors="replace"
    ) as stderr_file:
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
            while time.time() < deadline:
                text = log_path.read_text(encoding="utf-8", errors="replace") if log_path.exists() else ""
                if any(marker in text for marker in stop_markers):
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


def require_all(text: str, label: str, needles: tuple[str, ...]) -> None:
    for needle in needles:
        if needle not in text:
            raise RuntimeError(f"{label} missing expected output: {needle}")


def require_any(text: str, label: str, needles: tuple[str, ...]) -> None:
    if any(needle in text for needle in needles):
        return
    raise RuntimeError(f"{label} missing expected output from: {', '.join(needles)}")


def require_absent(text: str, label: str, needles: tuple[str, ...]) -> None:
    for needle in needles:
        if needle in text:
            raise RuntimeError(f"{label} observed forbidden output: {needle}")


def verify_target_image(label: str, expect_success: bool = True) -> str:
    completed = subprocess.run(
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
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if expect_success and completed.returncode != 0:
        detail = (completed.stderr or completed.stdout).strip()
        raise RuntimeError(f"{label} host verify failed: {detail}")
    if not expect_success and completed.returncode == 0:
        raise RuntimeError(f"{label} unexpectedly verified an incomplete native layout")
    if not expect_success:
        detail = (completed.stderr or completed.stdout).strip().splitlines()
        reason = f" reason={detail[0]}" if detail else ""
        sys.stdout.write(f"installer.verify target={TARGET_IMAGE} result=rejected{reason}\n")
        return completed.stdout
    sys.stdout.write(completed.stdout)
    return completed.stdout


def installer_args(qemu: str, ovmf_code: str, vars_path: Path) -> list[str]:
    return [
        qemu,
        "-machine",
        "q35",
        "-device",
        "virtio-keyboard-pci",
        "-drive",
        f"if=pflash,format=raw,readonly=on,file={ovmf_code}",
        "-drive",
        f"if=pflash,format=raw,file={vars_path}",
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
    ]


def boot_args(qemu: str, ovmf_code: str, vars_path: Path) -> list[str]:
    return [
        qemu,
        "-machine",
        "q35",
        "-device",
        "virtio-keyboard-pci",
        "-drive",
        f"if=pflash,format=raw,readonly=on,file={ovmf_code}",
        "-drive",
        f"if=pflash,format=raw,file={vars_path}",
        "-drive",
        f"format=raw,file={TARGET_IMAGE}",
        "-serial",
        "stdio",
        "-display",
        "none",
        "-no-reboot",
        "-no-shutdown",
    ]


def main() -> int:
    qemu = find_qemu()
    ovmf_code = find_ovmf_code()
    vars_template = find_ovmf_vars_template()

    cleanup_paths(
        TARGET_IMAGE,
        FAILURE_LOG,
        FAILURE_ERR,
        SUCCESS_LOG,
        SUCCESS_ERR,
        IDEMPOTENT_LOG,
        IDEMPOTENT_ERR,
        BOOT_LOG,
        BOOT_ERR,
        FAILURE_VARS,
        SUCCESS_VARS,
        IDEMPOTENT_VARS,
        BOOT_VARS,
    )
    create_target(TARGET_IMAGE, FAILURE_TARGET_SIZE)
    shutil.copyfile(vars_template, FAILURE_VARS)
    shutil.copyfile(vars_template, SUCCESS_VARS)
    shutil.copyfile(vars_template, IDEMPOTENT_VARS)
    shutil.copyfile(vars_template, BOOT_VARS)

    failure = run_qemu_capture(
        installer_args(qemu, ovmf_code, FAILURE_VARS),
        FAILURE_LOG,
        FAILURE_ERR,
        ("[SYSTEM] installer gate=blocked", "[INSTALLER] verify ok"),
        60.0,
    )
    require_all(
        failure,
        "installer failure",
        (
            "[BOOT] installer media mode",
            "[INSTALLER] target bind ok",
            "[INSTALLER] writer identity ok",
            "[SYSTEM] installer gate=blocked",
        ),
    )
    require_any(
        failure,
        "installer failure",
        (
            "[INSTALLER] gpt backup entries fail",
            "[INSTALLER] gpt backup header fail",
        ),
    )
    require_absent(
        failure,
        "installer failure",
        (
            "[INSTALLER] gpt mbr fail",
            "[INSTALLER] gpt header fail",
            "[INSTALLER] gpt entries fail",
            "[INSTALLER] esp write ok",
            "[INSTALLER] lsys write ok",
            "[INSTALLER] ldat write ok",
            "[INSTALLER] lrcv write ok",
            "[INSTALLER] verify ok",
        ),
    )
    require_absent(failure, "installer failure", INSTALLER_PRODUCT_SPACE_MARKERS)
    verify_target_image("installer undersized failure", expect_success=False)

    resize_target(TARGET_IMAGE, RECOVERY_TARGET_SIZE)
    recovery = run_qemu_capture(
        installer_args(qemu, ovmf_code, SUCCESS_VARS),
        SUCCESS_LOG,
        SUCCESS_ERR,
        ("[INSTALLER] verify ok", "[SYSTEM] installer gate=blocked"),
        60.0,
    )
    require_all(
        recovery,
        "installer recovery",
        (
            "[BOOT] installer media mode",
            "[INSTALLER] target bind ok",
            "[INSTALLER] writer identity ok",
            "[INSTALLER] esp write ok",
            "[INSTALLER] lsys write ok",
            "[INSTALLER] ldat write ok",
            "[INSTALLER] lrcv write ok",
            "[INSTALLER] lrcv verify ok",
            "[INSTALLER] verify ok",
        ),
    )
    require_absent(
        recovery,
        "installer recovery",
        (
            "[SYSTEM] installer gate=blocked",
            "[INSTALLER] gpt mbr fail",
            "[INSTALLER] gpt header fail",
            "[INSTALLER] gpt entries fail",
            "[INSTALLER] gpt backup entries fail",
            "[INSTALLER] gpt backup header fail",
            "[INSTALLER] esp target write fail",
            "[INSTALLER] lsys super fail",
            "[INSTALLER] lsys meta fail",
            "[INSTALLER] ldat super fail",
            "[INSTALLER] ldat meta fail",
            "[INSTALLER] lrcv write fail",
            "[INSTALLER] verify lrcv read fail",
            "[INSTALLER] verify lrcv contract fail",
            "[INSTALLER] verify gpt read fail",
            "[INSTALLER] verify lsys read fail",
            "[INSTALLER] verify lsys contract fail",
            "[INSTALLER] verify ldat read fail",
            "[INSTALLER] verify ldat contract fail",
        ),
    )
    baseline_verify = verify_target_image("installer recovery")

    idempotent = run_qemu_capture(
        installer_args(qemu, ovmf_code, IDEMPOTENT_VARS),
        IDEMPOTENT_LOG,
        IDEMPOTENT_ERR,
        ("[INSTALLER] verify ok", "[SYSTEM] installer gate=blocked"),
        60.0,
    )
    require_all(
        idempotent,
        "installer idempotent",
        (
            "[BOOT] installer media mode",
            "[INSTALLER] target bind ok",
            "[INSTALLER] writer identity ok",
            "[INSTALLER] esp write ok",
            "[INSTALLER] lsys write ok",
            "[INSTALLER] ldat write ok",
            "[INSTALLER] lrcv write ok",
            "[INSTALLER] lrcv verify ok",
            "[INSTALLER] verify ok",
        ),
    )
    require_absent(
        idempotent,
        "installer idempotent",
        (
            "[SYSTEM] installer gate=blocked",
            "[INSTALLER] gpt mbr fail",
            "[INSTALLER] gpt header fail",
            "[INSTALLER] gpt entries fail",
            "[INSTALLER] gpt backup entries fail",
            "[INSTALLER] gpt backup header fail",
            "[INSTALLER] esp target write fail",
            "[INSTALLER] lsys super fail",
            "[INSTALLER] lsys meta fail",
            "[INSTALLER] ldat super fail",
            "[INSTALLER] ldat meta fail",
            "[INSTALLER] lrcv write fail",
            "[INSTALLER] verify lrcv read fail",
            "[INSTALLER] verify lrcv contract fail",
            "[INSTALLER] verify gpt read fail",
            "[INSTALLER] verify lsys read fail",
            "[INSTALLER] verify lsys contract fail",
            "[INSTALLER] verify ldat read fail",
            "[INSTALLER] verify ldat contract fail",
        ),
    )
    if verify_target_image("installer idempotent") != baseline_verify:
        raise RuntimeError("installer idempotency changed the verified native layout")

    boot = run_qemu_capture(
        boot_args(qemu, ovmf_code, BOOT_VARS),
        BOOT_LOG,
        BOOT_ERR,
        ("[USER] shell ready", "[SYSTEM] installer gate=blocked"),
        60.0,
    )
    require_all(
        boot,
        "installer boot",
        (
            "[BOOT] dawn online",
            "[BOOT] fwblk handoff ok",
            "[BOOT] lsys super read ok",
            "[BOOT] native pair ok",
            "[BOOT] continuing boot",
            "[USER] shell ready",
            "first-setup required: no hostname or user configured",
        ),
    )
    require_absent(
        boot,
        "installer boot",
        (
            "Exception Type",
            "Can't find image information",
            "[SYSTEM] installer gate=blocked",
            "[FWBLK] handoff missing",
            "[BOOT] lsys read fail",
            "[BOOT] lsys contract fail",
            "[BOOT] ldat read fail",
            "[BOOT] ldat contract fail",
        ),
    )

    sys.stdout.write(failure)
    sys.stdout.write("\n--- RECOVERY RETRY ---\n")
    sys.stdout.write(recovery)
    sys.stdout.write("\n--- IDEMPOTENT RETRY ---\n")
    sys.stdout.write(idempotent)
    sys.stdout.write("\n--- TARGET BOOT ---\n")
    sys.stdout.write(boot)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
