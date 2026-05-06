from __future__ import annotations

import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent
IMAGE = ROOT / "lunaos.img"
CHECK_IMAGE = ROOT / "lunaos_updateapplycheck.img"
LOG1_PATH = ROOT / "qemu_updateapplycheck_boot1.log"
ERR1_PATH = ROOT / "qemu_updateapplycheck_boot1.err.log"
LOG2_PATH = ROOT / "qemu_updateapplycheck_boot2.log"
ERR2_PATH = ROOT / "qemu_updateapplycheck_boot2.err.log"
LOG3_PATH = ROOT / "qemu_updateapplycheck_boot3.log"
ERR3_PATH = ROOT / "qemu_updateapplycheck_boot3.err.log"
LOG4_PATH = ROOT / "qemu_updateapplycheck_boot4.log"
ERR4_PATH = ROOT / "qemu_updateapplycheck_boot4.err.log"
BOOT_SECTOR_BYTES = 512
SESSION_MAGIC = 0x54504353
DEV_BUNDLE_STAGE_MAGIC = 0x4244564C


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


def patch_session_script(image_path: Path, commands: bytes, staged_bundle: bytes = b"") -> None:
    image_base, script_base, script_size = parse_session_layout()
    payload_capacity = script_size - 8
    extra = b""
    if staged_bundle:
        extra = struct.pack("<II", DEV_BUNDLE_STAGE_MAGIC, len(staged_bundle)) + staged_bundle
    if len(commands) + len(extra) > payload_capacity:
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
    payload = header + commands + extra + (b"\x00" * (script_size - len(header) - len(commands) - len(extra)))
    blob[offset:offset + script_size] = payload
    image_path.write_bytes(blob)


def pack_devloop_bundle() -> bytes:
    repo_root = ROOT.parent
    output = ROOT / "devloop_updateapply_sample.luna"
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


def run_vm(
    image_path: Path,
    log_path: Path,
    err_path: Path,
    stop_markers: list[str],
    timeout_s: float,
) -> str:
    qemu = find_qemu()
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
            deadline = time.time() + timeout_s
            while time.time() < deadline:
                if log_path.exists():
                    text = log_path.read_text(encoding="utf-8", errors="replace")
                    if all(marker in text for marker in stop_markers):
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


def require_absent(text: str, label: str, needles: tuple[str, ...]) -> None:
    for needle in needles:
        if needle in text:
            raise RuntimeError(f"{label} observed forbidden output: {needle}")


def main() -> int:
    bundle = pack_devloop_bundle()
    CHECK_IMAGE.write_bytes(IMAGE.read_bytes())

    boot1_commands = (
        b"setup.init luna dev secret\r\n"
        b"package.install sample\r\n"
        b"update.status\r\n"
    )
    patch_session_script(CHECK_IMAGE, boot1_commands, bundle)
    boot1 = run_vm(
        CHECK_IMAGE,
        LOG1_PATH,
        ERR1_PATH,
        ["package.install ok", "update action=apply-ready"],
        30.0,
    )

    boot2_commands = (
        b"login dev secret\r\n"
        b"update.status\r\n"
        b"update.apply\r\n"
    )
    patch_session_script(CHECK_IMAGE, boot2_commands)
    boot2 = run_vm(
        CHECK_IMAGE,
        LOG2_PATH,
        ERR2_PATH,
        ["update.apply ok"],
        50.0,
    )

    boot3_commands = b""
    patch_session_script(CHECK_IMAGE, boot3_commands)
    boot3 = run_vm(
        CHECK_IMAGE,
        LOG3_PATH,
        ERR3_PATH,
        ["audit update.apply activation=LSYS", "audit update.apply persisted=DATA authority=UPDATE"],
        25.0,
    )

    boot4_commands = (
        b"login dev secret\r\n"
        b"update.status\r\n"
        b"list-apps\r\n"
        b"run sample\r\n"
    )
    patch_session_script(CHECK_IMAGE, boot4_commands)
    boot4 = run_vm(
        CHECK_IMAGE,
        LOG4_PATH,
        ERR4_PATH,
        ["update action=applied", "launch request: sample.luna"],
        40.0,
    )

    require_all(
        boot1,
        "updateapply boot1",
        (
            "package.install sample",
            "package.install ok",
            "audit package.install approved=SECURITY",
            "audit package.install persisted=DATA authority=PACKAGE",
            "update.status",
            "update mode=run action=apply-ready",
            "update state=staged current=1 target=2",
            "update action=apply-ready",
        ),
    )
    require_absent(
        boot1,
        "updateapply boot1",
        (
            "update.apply fail",
            "update.apply ok",
        ),
    )

    require_all(
        boot2,
        "updateapply boot2",
        (
            "login ok: session active",
            "update.status",
            "update mode=run action=apply-ready",
            "update state=staged current=1 target=2",
            "update.apply",
            "audit update.apply start",
            "audit package.remove approved=SECURITY",
            "audit package.remove persisted=DATA authority=PACKAGE",
            "audit package.install approved=SECURITY",
            "audit package.install persisted=DATA authority=PACKAGE",
            "audit update.apply committed=UPDATE",
            "audit update.apply activation=COMMITTED",
            "update.apply ok",
            "update.result state=committed current=1 target=2",
        ),
    )
    require_absent(
        boot2,
        "updateapply boot2",
        (
            "update.apply fail",
            "audit update.apply denied reason=package-chain",
            "audit update.apply denied reason=txn-log",
        ),
    )

    require_all(
        boot3,
        "updateapply boot3",
        (
            "audit update.apply activation=LSYS",
            "audit update.apply persisted=DATA authority=UPDATE",
        ),
    )
    require_absent(
        boot3,
        "updateapply boot3",
        (
            "update.apply fail",
            "audit update.apply denied reason=package-chain",
            "audit update.apply denied reason=txn-log",
            "login ok: session active",
            "[USER] shell ready",
        ),
    )

    require_all(
        boot4,
        "updateapply boot4",
        (
            "login ok: session active",
            "update.status",
            "update mode=read-only action=applied",
            "update state=active current=2 target=2",
            "update action=applied",
            "list-apps",
            "sample.luna",
            "run sample",
            "launch request: sample.luna",
            "apps 1-5 or F/N/G/C/H",
        ),
    )
    require_absent(
        boot4,
        "updateapply boot4",
        (
            "update.apply fail",
            "audit update.apply denied reason=package-chain",
            "audit update.apply denied reason=txn-log",
        ),
    )

    sys.stdout.write(boot1)
    sys.stdout.write("\n--- APPLY COMMIT ---\n")
    sys.stdout.write(boot2)
    sys.stdout.write("\n--- ACTIVATION REBOOT ---\n")
    sys.stdout.write(boot3)
    sys.stdout.write("\n--- ACTIVATION CONFIRM ---\n")
    sys.stdout.write(boot4)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
