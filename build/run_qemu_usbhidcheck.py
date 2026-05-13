from __future__ import annotations

import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

from run_qemu_userinputcheck import (
    DISK,
    ROOT,
    QmpSession,
    cleanup_paths,
    find_ovmf_code,
    find_ovmf_vars_template,
    find_qemu,
    patch_session_script,
    qmp_qcode_for_char,
)


USBHID_DISK = ROOT / "lunaos_usbhidcheck.img"
LOG_PATH = ROOT / "qemu_usbhidcheck.log"
ERR_PATH = ROOT / "qemu_usbhidcheck.err.log"
VARS_PATH = ROOT / "ovmf-usbhidcheck-vars.fd"


def strip_ansi(text: str) -> str:
    return re.sub(r"\x1B\[[0-9;?=]*[ -/]*[@-~]", "", text)


def read_log() -> str:
    if not LOG_PATH.exists():
        return ""
    return strip_ansi(LOG_PATH.read_text(encoding="utf-8", errors="replace"))


def wait_for_log(required: list[str], timeout_seconds: float, forbidden: list[str] | None = None) -> str:
    deadline = time.time() + timeout_seconds
    forbidden = forbidden or []
    while time.time() < deadline:
        text = read_log()
        for needle in forbidden:
            if needle in text:
                raise RuntimeError(f"usbhidcheck observed failure output: {needle}")
        if all(needle in text for needle in required):
            return text
        time.sleep(0.02)
    text = read_log()
    for needle in forbidden:
        if needle in text:
            raise RuntimeError(f"usbhidcheck observed failure output: {needle}")
    missing = next((needle for needle in required if needle not in text), required[0] if required else "<none>")
    raise RuntimeError(f"usbhidcheck missing expected output: {missing}")


def send_keys_qmp(qmp: QmpSession, text: str) -> None:
    for ch in text:
        qmp.execute("human-monitor-command", {"command-line": f"sendkey {qmp_qcode_for_char(ch)}"})
        time.sleep(0.08)


def main() -> int:
    qemu = find_qemu()
    ovmf_code = find_ovmf_code()
    vars_template = find_ovmf_vars_template()

    cleanup_paths(LOG_PATH, ERR_PATH, USBHID_DISK, VARS_PATH)
    shutil.copyfile(DISK, USBHID_DISK)
    shutil.copyfile(vars_template, VARS_PATH)
    patch_session_script(USBHID_DISK, b"")

    forbidden = [
        "Exception Type",
        "Can't find image information",
        "[USER] session script cmd=",
        "[DEVICE] input event src=i8042-kbd",
        "[DEVICE] input event src=virtio-kbd",
        "[DEVICE] input event src=serial-operator",
        "[USER] input lane src=operator",
        "[USER] shell accept src=operator",
        "[USER] shell execute src=operator",
        "[USER] input recovery=operator-shell",
    ]

    proc: subprocess.Popen[str] | None = None
    qmp: QmpSession | None = None
    with ERR_PATH.open("w", encoding="utf-8", errors="replace") as stderr_file:
        proc = subprocess.Popen(
            [
                qemu,
                "-machine",
                "q35,i8042=off",
                "-drive",
                f"if=pflash,format=raw,readonly=on,file={ovmf_code}",
                "-drive",
                f"if=pflash,format=raw,file={VARS_PATH}",
                "-drive",
                f"format=raw,file={USBHID_DISK}",
                "-device",
                "qemu-xhci,id=xhci",
                "-device",
                "usb-kbd,bus=xhci.0",
                "-serial",
                f"file:{LOG_PATH.name}",
                "-qmp",
                "stdio",
                "-display",
                "none",
                "-name",
                "LunaOS USB-HID Blocker Check",
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
                    "usb-ctrl=ready",
                    "usb-hid=not-bound",
                    "[DEVICE] input ctrl legacy=missing",
                    "[DEVICE] input select basis=usb-hid",
                    "[XHCI] controller ready",
                    "[XHCI] port ready",
                    "[XHCI] slot enabled",
                    "[XHCI] address device ready",
                    "[USB] device descriptor ready",
                    "[USB] config descriptor ready",
                    "[USB] hid keyboard interface ready",
                    "[USB] hid descriptor ready",
                    "[USB] interrupt endpoint ready",
                    "[USB] configuration set value=",
                    "[USB] interrupt poll armed",
                    "[DEVICE] input usb candidate ctrl=xhci hid=not-bound blocker=",
                    "[DEVICE] input usb host ctrl=xhci init=ready",
                    "port-ready=ready",
                    "[DEVICE] input usb pci ",
                    "class=0C/03/30",
                    "[USER] shell ready",
                    "[USER] input lane ready",
                ],
                45.0,
                forbidden=forbidden,
            )
            if "[USER] session script cmd=" in boot_text:
                raise RuntimeError("usbhidcheck unexpectedly used session script input")

            send_keys_qmp(qmp, "help\r")
            wait_for_log(
                [
                    "[USB] HID interrupt report received",
                    "[DEVICE] input event src=usb-hid key=68",
                    "[USER] input lane src=keyboard key=68",
                    "[USER] shell accept src=keyboard cmd=help",
                    "[USER] shell execute src=keyboard cmd=help",
                    "core: setup.status",
                ],
                20.0,
                forbidden=forbidden,
            )
        finally:
            if proc is not None and proc.poll() is None:
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

    stdout = read_log()
    stderr = ERR_PATH.read_text(encoding="utf-8", errors="replace") if ERR_PATH.exists() else ""
    for needle in forbidden:
        if needle in stdout or needle in stderr:
            raise RuntimeError(f"usbhidcheck observed unexpected output: {needle}")
    for match in re.finditer(r"\[DEVICE\] input event src=([^ ]+)", stdout):
        if match.group(1) != "usb-hid":
            raise RuntimeError(f"usbhidcheck observed non-USB-HID input source: {match.group(1)}")
    if "[DEVICE] input event src=usb-hid key=68" not in stdout:
        raise RuntimeError("usbhidcheck missing USB-HID DEVICE event for help")
    if "[USER] shell execute src=keyboard cmd=help" not in stdout:
        raise RuntimeError("usbhidcheck missing USB-HID-backed help execution")

    sys.stdout.write(stdout)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
