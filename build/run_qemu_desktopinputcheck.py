from __future__ import annotations

import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

from run_qemu_userinputcheck import (
    DISK,
    REAL_KEYBOARD_SOURCE_PATTERN,
    ROOT,
    QmpSession,
    cleanup_paths,
    find_ovmf_code,
    find_ovmf_vars_template,
    find_qemu,
    patch_session_script,
    qmp_qcode_for_char,
)


DESKTOPINPUT_DISK = ROOT / "lunaos_desktopinputcheck.img"
LOG_PATH = ROOT / "qemu_desktopinputcheck.log"
SETUP_LOG_PATH = ROOT / "qemu_desktopinputcheck.setup.log"
RUN_LOG_PATH = ROOT / "qemu_desktopinputcheck.run.log"
ERR_PATH = ROOT / "qemu_desktopinputcheck.err.log"
VARS_PATH = ROOT / "ovmf-desktopinputcheck-vars.fd"


def strip_ansi(text: str) -> str:
    return re.sub(r"\x1B\[[0-9;?=]*[ -/]*[@-~]", "", text)


def read_log(path: Path) -> str:
    if not path.exists():
        return ""
    return strip_ansi(path.read_text(encoding="utf-8", errors="replace"))


def wait_for_log(
    log_path: Path,
    required: list[str],
    timeout_seconds: float,
    forbidden: list[str] | None = None,
) -> str:
    deadline = time.time() + timeout_seconds
    forbidden = forbidden or []
    while time.time() < deadline:
        text = read_log(log_path)
        for needle in forbidden:
            if needle in text:
                raise RuntimeError(f"desktopinputcheck observed failure output: {needle}")
        if all(needle in text for needle in required):
            return text
        time.sleep(0.01)
    text = read_log(log_path)
    for needle in forbidden:
        if needle in text:
            raise RuntimeError(f"desktopinputcheck observed failure output: {needle}")
    missing = next((needle for needle in required if needle not in text), required[0] if required else "<none>")
    raise RuntimeError(f"desktopinputcheck missing expected output: {missing}")


def wait_for_match(
    log_path: Path,
    pattern: str,
    timeout_seconds: float,
    forbidden: list[str] | None = None,
) -> re.Match[str]:
    compiled = re.compile(pattern)
    deadline = time.time() + timeout_seconds
    forbidden = forbidden or []
    while time.time() < deadline:
        text = read_log(log_path)
        for needle in forbidden:
            if needle in text:
                raise RuntimeError(f"desktopinputcheck observed failure output: {needle}")
        match = compiled.search(text)
        if match is not None:
            return match
        time.sleep(0.01)
    text = read_log(log_path)
    for needle in forbidden:
        if needle in text:
            raise RuntimeError(f"desktopinputcheck observed failure output: {needle}")
    raise RuntimeError(f"desktopinputcheck missing expected match: {pattern}")


def wait_for_ordered_patterns(
    log_path: Path,
    patterns: list[str],
    timeout_seconds: float,
    description: str,
    forbidden: list[str] | None = None,
) -> str:
    deadline = time.time() + timeout_seconds
    forbidden = forbidden or []
    while True:
        text = read_log(log_path)
        for needle in forbidden:
            if needle in text:
                raise RuntimeError(f"desktopinputcheck observed failure output: {needle}")
        offset = 0
        matched = True
        for pattern in patterns:
            match = re.search(pattern, text[offset:])
            if match is None:
                matched = False
                break
            offset += match.end()
        if matched:
            return text
        if time.time() >= deadline:
            break
        time.sleep(0.01)
    text = read_log(log_path)
    for needle in forbidden:
        if needle in text:
            raise RuntimeError(f"desktopinputcheck observed failure output: {needle}")
    raise RuntimeError(f"desktopinputcheck missing ordered real keyboard path: {description}")


def wait_for_log_idle(
    log_path: Path,
    stable_seconds: float,
    timeout_seconds: float,
    forbidden: list[str] | None = None,
) -> str:
    deadline = time.time() + timeout_seconds
    forbidden = forbidden or []
    last_text = read_log(log_path)
    stable_since = time.time()
    while time.time() < deadline:
        text = read_log(log_path)
        for needle in forbidden:
            if needle in text:
                raise RuntimeError(f"desktopinputcheck observed failure output: {needle}")
        if len(text) != len(last_text):
            last_text = text
            stable_since = time.time()
        elif time.time() - stable_since >= stable_seconds:
            return text
        time.sleep(0.02)
    return read_log(log_path)


def key_hex_for_char(ch: str) -> str:
    if ch in ("\r", "\n"):
        return "0A"
    return f"{ord(ch):02X}"


def wait_for_marker_count(
    log_path: Path,
    marker: str,
    minimum_count: int,
    timeout_seconds: float,
    forbidden: list[str],
) -> str:
    deadline = time.time() + timeout_seconds
    while time.time() < deadline:
        text = read_log(log_path)
        for needle in forbidden:
            if needle in text:
                raise RuntimeError(f"desktopinputcheck observed failure output: {needle}")
        if text.count(marker) >= minimum_count:
            return text
        time.sleep(0.01)
    text = read_log(log_path)
    if text.count(marker) < minimum_count:
        raise RuntimeError(f"desktopinputcheck missing synchronized keyboard evidence: {marker}")
    return text


def wait_for_pattern_count(
    log_path: Path,
    pattern: str,
    minimum_count: int,
    timeout_seconds: float,
    forbidden: list[str],
) -> str:
    compiled = re.compile(pattern)
    deadline = time.time() + timeout_seconds
    while time.time() < deadline:
        text = read_log(log_path)
        for needle in forbidden:
            if needle in text:
                raise RuntimeError(f"desktopinputcheck observed failure output: {needle}")
        if len(compiled.findall(text)) >= minimum_count:
            return text
        time.sleep(0.01)
    text = read_log(log_path)
    if len(compiled.findall(text)) < minimum_count:
        raise RuntimeError(f"desktopinputcheck missing synchronized keyboard evidence: {pattern}")
    return text


def wait_for_keyboard_command_path(
    log_path: Path,
    first_key_hex: str,
    command: str,
    timeout_seconds: float,
    forbidden: list[str],
) -> str:
    escaped_command = re.escape(command)
    return wait_for_ordered_patterns(
        log_path,
        [
            rf"\[DEVICE\] input event src={REAL_KEYBOARD_SOURCE_PATTERN} key={first_key_hex}",
            rf"\[USER\] input lane src=keyboard key={first_key_hex}",
            rf"\[USER\] shell accept src=keyboard cmd={escaped_command}",
            rf"\[USER\] shell execute src=keyboard cmd={escaped_command}",
        ],
        timeout_seconds,
        f"key={first_key_hex} command={command}",
        forbidden=forbidden,
    )


def send_key_qmp(qmp: QmpSession, qcode: str) -> None:
    qmp.execute("human-monitor-command", {"command-line": f"sendkey {qcode} 350"})
    time.sleep(0.42)


def send_keys_qmp(qmp: QmpSession, text: str, log_path: Path, forbidden: list[str]) -> None:
    wait_for_log_idle(log_path, 0.45, 5.0, forbidden=forbidden)
    for ch in text:
        key_hex = key_hex_for_char(ch)
        device_pattern = rf"\[DEVICE\] input event src={REAL_KEYBOARD_SOURCE_PATTERN} key={key_hex}"
        user_marker = f"[USER] input lane src=keyboard key={key_hex}"
        last_error: Exception | None = None
        for _ in range(3):
            snapshot = read_log(log_path)
            device_target = len(re.findall(device_pattern, snapshot)) + 1
            user_target = snapshot.count(user_marker) + 1
            send_key_qmp(qmp, qmp_qcode_for_char(ch))
            try:
                wait_for_pattern_count(log_path, device_pattern, device_target, 6.0, forbidden)
                wait_for_marker_count(log_path, user_marker, user_target, 4.0, forbidden)
                last_error = None
                break
            except RuntimeError as exc:
                last_error = exc
        if last_error is not None:
            raise last_error
        time.sleep(0.08)


def start_qemu(
    qemu: str,
    ovmf_code: str,
    log_path: Path,
    name: str,
    stderr_file,
) -> subprocess.Popen[str]:
    return subprocess.Popen(
        [
            qemu,
            "-machine",
            "q35",
            "-drive",
            f"if=pflash,format=raw,readonly=on,file={ovmf_code}",
            "-drive",
            f"if=pflash,format=raw,file={VARS_PATH}",
            "-drive",
            f"format=raw,file={DESKTOPINPUT_DISK}",
            "-serial",
            f"file:{log_path.name}",
            "-qmp",
            "stdio",
            "-display",
            "none",
            "-name",
            name,
            "-no-reboot",
            "-no-shutdown",
        ],
        cwd=ROOT,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=stderr_file,
        text=True,
        encoding="utf-8",
        errors="replace",
    )


def stop_qemu(proc: subprocess.Popen[str], qmp: QmpSession | None) -> None:
    if proc.poll() is not None:
        return
    if qmp is not None:
        try:
            qmp.execute("quit")
        except Exception:
            pass
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=5)


def validate_no_script_or_operator(text: str, stderr: str, forbidden: list[str]) -> None:
    for needle in forbidden:
        if needle in text or needle in stderr:
            raise RuntimeError(f"desktopinputcheck observed unexpected output: {needle}")


def run_setup_boot(qemu: str, ovmf_code: str, forbidden: list[str], stderr_file) -> None:
    proc: subprocess.Popen[str] | None = None
    qmp: QmpSession | None = None
    proc = start_qemu(qemu, ovmf_code, SETUP_LOG_PATH, "LunaOS Desktop Input Setup", stderr_file)
    try:
        qmp = QmpSession(proc)
        boot_text = wait_for_log(
            SETUP_LOG_PATH,
            [
                "LunaLoader UEFI Stage 1 handoff",
                "[BOOT] dawn online",
                "[DEVICE] input path kbd=",
                "[DEVICE] input select basis=",
                "usb-hid=not-bound",
                "usb-hid-blocker=controller-missing",
                "[USER] shell ready",
                "[USER] input lane ready",
                "first-setup required: no hostname or user configured",
                "guest@luna:~$",
            ],
            45.0,
            forbidden=forbidden,
        )
        if re.search(r"\[DEVICE\] input path kbd=(virtio-kbd|i8042-kbd)", boot_text) is None:
            raise RuntimeError("desktopinputcheck missing keyboard path selection during setup boot")
        if "setup.init h u p\r\n" in boot_text:
            raise RuntimeError("desktopinputcheck setup boot started with pre-seeded commands")

        send_keys_qmp(qmp, "setup.init h dev p\r", SETUP_LOG_PATH, forbidden)
        wait_for_keyboard_command_path(
            SETUP_LOG_PATH,
            "73",
            "setup.init h dev p",
            20.0,
            forbidden,
        )
        wait_for_log(
            SETUP_LOG_PATH,
            [
                "[USER] shell execute src=keyboard cmd=setup.init h dev p",
                "setup.init ok: host and first user created",
                "login ok: session active",
                "[USER] desktop session online",
            ],
            20.0,
            forbidden=forbidden,
        )
        time.sleep(0.5)
    finally:
        if proc is not None:
            stop_qemu(proc, qmp)


def run_desktop_input_boot(qemu: str, ovmf_code: str, forbidden: list[str], stderr_file) -> None:
    proc: subprocess.Popen[str] | None = None
    qmp: QmpSession | None = None
    proc = start_qemu(qemu, ovmf_code, RUN_LOG_PATH, "LunaOS Desktop Input Check", stderr_file)
    try:
        qmp = QmpSession(proc)
        boot_text = wait_for_log(
            RUN_LOG_PATH,
            [
                "LunaLoader UEFI Stage 1 handoff",
                "[BOOT] dawn online",
                "[DEVICE] input path kbd=",
                "[DEVICE] input select basis=",
                "[USER] shell ready",
                "[USER] input lane ready",
                "login required: session is locked",
                "login <username> <password>",
                "guest@h:~$",
            ],
            45.0,
            forbidden=forbidden,
        )
        if "first-setup required: no hostname or user configured" in boot_text:
            raise RuntimeError("desktopinputcheck setup state was not persisted before desktop input boot")
        if "login u p\r\n" in boot_text:
            raise RuntimeError("desktopinputcheck input boot started with pre-seeded commands")

        send_keys_qmp(qmp, "login dev p\r", RUN_LOG_PATH, forbidden)
        wait_for_keyboard_command_path(
            RUN_LOG_PATH,
            "6C",
            "login dev p",
            20.0,
            forbidden,
        )
        wait_for_log(
            RUN_LOG_PATH,
            [
                "[USER] shell execute src=keyboard cmd=login dev p",
                "login ok: session active",
                "[USER] desktop session online",
            ],
            20.0,
            forbidden=forbidden,
        )

        send_keys_qmp(qmp, "f", RUN_LOG_PATH, forbidden)
        wait_for_match(
            RUN_LOG_PATH,
            rf"\[DEVICE\] input event src={REAL_KEYBOARD_SOURCE_PATTERN} key=66",
            15.0,
            forbidden=forbidden,
        )
        wait_for_log(
            RUN_LOG_PATH,
            [
                "[USER] input lane src=keyboard key=66",
                "[USER] desktop key=66",
            ],
            15.0,
            forbidden=forbidden,
        )
        time.sleep(0.2)
    finally:
        if proc is not None:
            stop_qemu(proc, qmp)


def main() -> int:
    qemu = find_qemu()
    ovmf_code = find_ovmf_code()
    vars_template = find_ovmf_vars_template()

    cleanup_paths(LOG_PATH, SETUP_LOG_PATH, RUN_LOG_PATH, ERR_PATH, DESKTOPINPUT_DISK, VARS_PATH)
    shutil.copyfile(DISK, DESKTOPINPUT_DISK)
    shutil.copyfile(vars_template, VARS_PATH)
    patch_session_script(DESKTOPINPUT_DISK, b"")

    forbidden_common = [
        "Exception Type",
        "Can't find image information",
        "[SYSTEM] driver gate=recovery",
        "[USER] input lane src=operator",
        "[DEVICE] input event src=serial-operator",
        "[USER] input recovery=operator-shell",
        "[USER] session script cmd=",
        "desktop.launch Files\r\n[USER] session script cmd=desktop.launch Files",
    ]

    with ERR_PATH.open("w", encoding="utf-8", errors="replace") as stderr_file:
        run_setup_boot(qemu, ovmf_code, forbidden_common, stderr_file)
        run_desktop_input_boot(qemu, ovmf_code, forbidden_common, stderr_file)

    setup_text = read_log(SETUP_LOG_PATH)
    run_text = read_log(RUN_LOG_PATH)
    stderr = ERR_PATH.read_text(encoding="utf-8", errors="replace") if ERR_PATH.exists() else ""
    combined = (
        "=== desktopinput setup boot ===\r\n"
        + setup_text
        + "\r\n=== desktopinput real keyboard boot ===\r\n"
        + run_text
    )
    LOG_PATH.write_text(combined, encoding="utf-8", errors="replace")
    validate_no_script_or_operator(combined, stderr, forbidden_common)

    required = [
        "[USER] input lane src=keyboard key=73",
        "[USER] shell accept src=keyboard cmd=setup.init h dev p",
        "[USER] shell execute src=keyboard cmd=setup.init h dev p",
        "setup.init ok: host and first user created",
        "[USER] input lane src=keyboard key=6C",
        "[USER] shell accept src=keyboard cmd=login dev p",
        "[USER] shell execute src=keyboard cmd=login dev p",
        "login ok: session active",
        "[USER] desktop session online",
        "[USER] input lane src=keyboard key=66",
        "[USER] desktop key=66",
    ]
    for needle in required:
        if needle not in combined:
            raise RuntimeError(f"desktopinputcheck missing expected output: {needle}")
    wait_for_keyboard_command_path(
        SETUP_LOG_PATH,
        "73",
        "setup.init h dev p",
        0.0,
        forbidden_common,
    )
    wait_for_keyboard_command_path(
        RUN_LOG_PATH,
        "6C",
        "login dev p",
        0.0,
        forbidden_common,
    )
    if re.search(rf"\[DEVICE\] input event src={REAL_KEYBOARD_SOURCE_PATTERN} key=66", combined) is None:
        raise RuntimeError("desktopinputcheck missing real keyboard event for desktop key f")
    if re.search(r"\[(?:VIRTKBD|PS2)\].*code=(?:0021|21)", combined) is None:
        raise RuntimeError("desktopinputcheck missing keyboard make-code evidence for desktop key f")

    sys.stdout.write(combined)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
