from __future__ import annotations

import re
import shutil
import struct
import subprocess
import sys
import time
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parent
DISK = ROOT / "lunaos_disk.img"
USERINPUT_DISK = ROOT / "lunaos_userinputcheck.img"
LOG_PATH = ROOT / "qemu_userinputcheck.log"
ERR_PATH = ROOT / "qemu_userinputcheck.err.log"
VARS_PATH = ROOT / "ovmf-userinputcheck-vars.fd"

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

REAL_KEYBOARD_SOURCE_PATTERN = r"(virtio-kbd|i8042-kbd)"


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


def read_log() -> str:
    if not LOG_PATH.exists():
        return ""
    return strip_ansi(LOG_PATH.read_text(encoding="utf-8", errors="replace"))


def cleanup_paths(*paths: Path) -> None:
    for path in paths:
        if path.exists():
            path.unlink()


def wait_for_log(required: list[str], timeout_seconds: float, forbidden: list[str] | None = None) -> str:
    deadline = time.time() + timeout_seconds
    forbidden = forbidden or []
    while time.time() < deadline:
        text = read_log()
        for needle in forbidden:
            if needle in text:
                raise RuntimeError(f"userinputcheck observed failure output: {needle}")
        if all(needle in text for needle in required):
            return text
        time.sleep(0.01)
    text = read_log()
    for needle in forbidden:
        if needle in text:
            raise RuntimeError(f"userinputcheck observed failure output: {needle}")
    missing = next((needle for needle in required if needle not in text), required[0] if required else "<none>")
    raise RuntimeError(f"userinputcheck missing expected output: {missing}")


def wait_for_match(pattern: str, timeout_seconds: float, forbidden: list[str] | None = None) -> re.Match[str]:
    compiled = re.compile(pattern)
    deadline = time.time() + timeout_seconds
    forbidden = forbidden or []
    while time.time() < deadline:
        text = read_log()
        for needle in forbidden:
            if needle in text:
                raise RuntimeError(f"userinputcheck observed failure output: {needle}")
        match = compiled.search(text)
        if match is not None:
            return match
        time.sleep(0.01)
    text = read_log()
    for needle in forbidden:
        if needle in text:
            raise RuntimeError(f"userinputcheck observed failure output: {needle}")
    raise RuntimeError(f"userinputcheck missing expected match: {pattern}")


class QmpSession:
    def __init__(self, proc: subprocess.Popen[str]) -> None:
        if proc.stdin is None or proc.stdout is None:
            raise RuntimeError("userinputcheck QMP stdio unavailable")
        self._stdin = proc.stdin
        self._stdout = proc.stdout
        _ = self.read_message()
        self.execute("qmp_capabilities")

    def read_message(self) -> dict[str, object]:
        while True:
            line = self._stdout.readline()
            if line == "":
                raise RuntimeError("userinputcheck QMP connection closed")
            line = line.strip()
            if line:
                return json.loads(line)

    def execute(self, execute: str, arguments: dict[str, object] | None = None) -> dict[str, object]:
        return qmp_execute(self, execute, arguments)


def qmp_execute(qmp: QmpSession, execute: str, arguments: dict[str, object] | None = None) -> dict[str, object]:
    command: dict[str, object] = {"execute": execute}
    if arguments is not None:
        command["arguments"] = arguments
    qmp._stdin.write(json.dumps(command, separators=(",", ":")) + "\n")
    qmp._stdin.flush()
    while True:
        message = qmp.read_message()
        if "event" in message:
            continue
        if "error" in message:
            raise RuntimeError(f"userinputcheck QMP {execute} failed: {message['error']}")
        return message


def qmp_qcode_for_char(ch: str) -> str:
    if "a" <= ch <= "z" or "0" <= ch <= "9":
        return ch
    if ch == "\r" or ch == "\n":
        return "ret"
    if ch == " ":
        return "spc"
    if ch == ".":
        return "dot"
    if ch == "-":
        return "minus"
    raise RuntimeError(f"userinputcheck cannot encode QMP key: {ch!r}")


def send_key_qmp(qmp: QmpSession, qcode: str) -> None:
    qmp_execute(
        qmp,
        "human-monitor-command",
        {"command-line": f"sendkey {qcode}"},
    )
    time.sleep(0.08)


def send_keys_qmp(qmp: QmpSession, text: str) -> None:
    for ch in text:
        send_key_qmp(qmp, qmp_qcode_for_char(ch))
        time.sleep(0.04)


def main() -> int:
    qemu = find_qemu()
    ovmf_code = find_ovmf_code()
    vars_template = find_ovmf_vars_template()

    cleanup_paths(LOG_PATH, ERR_PATH, USERINPUT_DISK, VARS_PATH)
    shutil.copyfile(DISK, USERINPUT_DISK)
    shutil.copyfile(vars_template, VARS_PATH)
    patch_session_script(USERINPUT_DISK, b"")

    forbidden_common = [
        "Exception Type",
        "Can't find image information",
        "[SYSTEM] driver gate=recovery",
        "[USER] input lane src=operator",
        "[DEVICE] input event src=serial-operator",
        "[USER] input recovery=operator-shell",
        "[USER] session script cmd=",
    ]

    proc: subprocess.Popen[str] | None = None

    with ERR_PATH.open("w", encoding="utf-8", errors="replace") as stderr_file:
        proc = subprocess.Popen(
            [
                qemu,
                "-machine",
                "q35",
                "-drive",
                f"if=pflash,format=raw,readonly=on,file={ovmf_code}",
                "-drive",
                f"if=pflash,format=raw,file={VARS_PATH}",
                "-drive",
                f"format=raw,file={USERINPUT_DISK}",
                "-serial",
                f"file:{LOG_PATH.name}",
                "-qmp",
                "stdio",
                "-display",
                "gtk,show-cursor=on,grab-on-hover=on,zoom-to-fit=off",
                "-name",
                "LunaOS User Input Check",
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

        try:
            qmp = QmpSession(proc)
            boot_text = wait_for_log(
                [
                    "LunaLoader UEFI Stage 1 handoff",
                    "[BOOT] dawn online",
                    "[DEVICE] input path kbd=",
                    "[DEVICE] input select basis=",
                    "usb-hid=not-bound",
                    "[USER] shell ready",
                    "[USER] input lane ready",
                    "first-setup required: no hostname or user configured",
                ],
                45.0,
                forbidden=forbidden_common,
            )
            if re.search(r"\[DEVICE\] input path kbd=(virtio-kbd|i8042-kbd)", boot_text) is None:
                raise RuntimeError("userinputcheck missing keyboard path selection")
            if re.search(r"\[DEVICE\] input select basis=(virtio-kbd|i8042)", boot_text) is None:
                raise RuntimeError("userinputcheck missing keyboard driver selection")
            if "setup.status\r\n" in boot_text or "setup.init luna dev secret" in boot_text:
                raise RuntimeError("userinputcheck started with pre-seeded shell commands")

            send_keys_qmp(qmp, "help\r")
            wait_for_match(
                rf"\[DEVICE\] input event src=(?P<source>{REAL_KEYBOARD_SOURCE_PATTERN}) key=68",
                15.0,
                forbidden=forbidden_common,
            )
            wait_for_log(
                [
                    "[USER] input lane src=keyboard key=68",
                    "[USER] shell accept src=keyboard cmd=help",
                    "[USER] shell execute src=keyboard cmd=help",
                    "core: setup.status",
                ],
                15.0,
                forbidden=forbidden_common,
            )
        finally:
            if proc.poll() is None:
                proc.kill()
            proc.wait(timeout=5)

    stdout = read_log()
    stderr = ERR_PATH.read_text(encoding="utf-8", errors="replace") if ERR_PATH.exists() else ""

    required = [
        "LunaLoader UEFI Stage 1 handoff",
        "[BOOT] dawn online",
        "[DEVICE] input path kbd=",
        "[DEVICE] input select basis=",
        "[USER] shell ready",
        "[USER] input lane ready",
        "[USER] input lane src=keyboard key=68",
        "[USER] shell accept src=keyboard cmd=help",
        "[USER] shell execute src=keyboard cmd=help",
        "first-setup required: no hostname or user configured",
        "setup.init <hostname> <username> <password>",
        "core: setup.status",
    ]
    for needle in required:
        if needle not in stdout:
            raise RuntimeError(f"userinputcheck missing expected output: {needle}")

    if re.search(rf"\[DEVICE\] input event src={REAL_KEYBOARD_SOURCE_PATTERN} key=68", stdout) is None:
        raise RuntimeError("userinputcheck missing DEVICE real keyboard event for help")

    for needle in forbidden_common:
        if needle in stdout or needle in stderr:
            raise RuntimeError(f"userinputcheck observed unexpected output: {needle}")

    sys.stdout.write(stdout)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
