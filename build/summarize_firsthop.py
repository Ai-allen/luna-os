from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent

DEFAULT_LOG_CANDIDATES = (
    ROOT / "qemu_bootcheck.log",
    ROOT / "qemu_shellcheck.log",
    ROOT / "qemu_uefi_shellcheck.log",
    ROOT / "vmware_m1.serial.log",
)

SECTION_PATTERNS = {
    "boot": (
        "[BOOT] fwblk handoff ok",
        "[BOOT] lsys super read ok",
        "[BOOT] native pair ok",
        "[BOOT] continuing boot",
        "[BOOT] lsys read fail",
        "[FWBLK] handoff missing",
    ),
    "firmware": (
        "[DEVICE] fwblk source=",
        "[DEVICE] fwblk media src=",
    ),
    "storage": (
        "[DEVICE] disk path",
        "[DEVICE] disk select ",
        "[DEVICE] disk pci ",
        "[DEVICE] disk ahci port=",
        "[DISK] write fail",
        "[AHCI] ready fail",
        "[AHCI] idle fail",
        "[AHCI] status fail",
        "[ATA] ",
    ),
    "serial": (
        "[DEVICE] serial path",
        "[DEVICE] serial select ",
        "[DEVICE] serial pci ",
    ),
    "display": (
        "[DEVICE] display path",
        "[DEVICE] display select ",
        "[DEVICE] display pci ",
    ),
    "input": (
        "[DEVICE] input path",
        "[DEVICE] input select ",
        "[DEVICE] input pci ",
        "[DEVICE] input ctrl ",
        "[DEVICE] input usb candidate",
        "[DEVICE] input usb pci ",
        "[VIRTKBD] ready ",
        "[VIRTKBD] pci missing",
    ),
    "network": (
        "[DEVICE] net path",
        "[DEVICE] net select ",
        "[DEVICE] net pci ",
        "[DEVICE] net driver ",
        "[DEVICE] net link ",
        "[DEVICE] net mac=",
        "[DEVICE] net blocker=",
        "[DEVICE] net raw ",
    ),
    "platform": (
        "[DEVICE] platform pci ",
    ),
    "progress": (
        "[DEVICE] lane ready",
        "[USER] shell ready",
        "first-setup required",
        "[DESKTOP] status",
    ),
}


def strip_ansi(text: str) -> str:
    return re.sub(r"\x1B\[[0-9;?=]*[ -/]*[@-~]", "", text)


def pick_default_log() -> Path:
    for candidate in DEFAULT_LOG_CANDIDATES:
        if candidate.exists():
            return candidate
    raise FileNotFoundError("no default log found; pass a log path explicitly")


def collect_sections(lines: list[str]) -> dict[str, list[str]]:
    sections: dict[str, list[str]] = {name: [] for name in SECTION_PATTERNS}
    for line in lines:
        for section, prefixes in SECTION_PATTERNS.items():
            if any(prefix in line for prefix in prefixes):
                if line not in sections[section]:
                    sections[section].append(line)
    return sections


def summarize(log_path: Path) -> str:
    text = strip_ansi(log_path.read_text(encoding="utf-8", errors="replace"))
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    sections = collect_sections(lines)
    out: list[str] = []
    out.append(f"log: {log_path}")
    out.append("")
    for section in ("boot", "firmware", "storage", "serial", "display", "input", "network", "platform", "progress"):
        out.append(f"[{section}]")
        if sections[section]:
            out.extend(sections[section])
        else:
            out.append("(missing)")
        out.append("")
    return "\n".join(out).rstrip() + "\n"


def main() -> int:
    try:
        log_path = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else pick_default_log()
        if not log_path.exists():
            raise FileNotFoundError(log_path)
        sys.stdout.write(summarize(log_path))
        return 0
    except Exception as exc:
        sys.stderr.write(f"firsthop summary failed: {exc}\n")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
