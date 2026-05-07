from __future__ import annotations

import re
import sys
from pathlib import Path


def strip_ansi(text: str) -> str:
    return re.sub(r"\x1B\[[0-9;?=]*[ -/]*[@-~]", "", text)


def read_lines(log_path: Path) -> list[str]:
    text = strip_ansi(log_path.read_text(encoding="utf-8", errors="replace"))
    return [line.strip() for line in text.splitlines() if line.strip()]


def first_match(lines: list[str], prefix: str) -> str | None:
    for line in lines:
        if prefix in line:
            return line
    return None


def detect_firmware(lines: list[str]) -> str:
    if (
        first_match(lines, "LunaLoader UEFI Stage 1 handoff")
        or first_match(lines, "BdsDxe:")
        or first_match(lines, "[BOOT] fwblk magic=49464555")
    ):
        return "uefi"
    if first_match(lines, "[LunaLoader] Stage 1 online"):
        return "legacy"
    return "unknown"


def parse_driver(line: str | None, key: str) -> str:
    if not line:
        return "missing"
    match = re.search(rf"{re.escape(key)}=([^ ]+)", line)
    if not match:
        return "unknown"
    return match.group(1)


def build_profile(*parts: tuple[str, str]) -> str:
    return ",".join(f"{key}={value}" for key, value in parts)


def input_identity(
    input_pci: str | None,
    input_ctrl: str | None,
    input_usb_candidate: str | None,
    input_usb_pci: str | None,
) -> str:
    parts = [part for part in (input_pci or input_ctrl, input_usb_candidate, input_usb_pci) if part]
    return " | ".join(parts) if parts else "(missing)"


TARGET_SUPPORT_CELL = "intel-x86_64+uefi+sata-ahci+gop+keyboard"
SUCCESS_PROGRESS = {
    "shell-ready",
    "shell-ready-first-setup",
    "desktop-shell-ready",
    "desktop-first-setup",
}


def parse_profile(profile: str) -> dict[str, str]:
    out: dict[str, str] = {}
    for part in profile.split(","):
        if "=" not in part:
            continue
        key, value = part.split("=", 1)
        out[key] = value
    return out


def support_cell_runtime_gate(info: dict[str, str]) -> tuple[str, str]:
    driver_profile = parse_profile(info["driver_profile"])
    selection_profile = parse_profile(info["selection_profile"])
    blockers: list[str] = []

    if info["firmware"] != "uefi":
        blockers.append("firmware-not-uefi")
    if driver_profile.get("storage") != "ahci" or selection_profile.get("storage") != "ahci-runtime":
        blockers.append("storage-not-ahci-runtime")
    if info["storage_lane"] != "ready":
        blockers.append("storage-lane-not-ready")
    if driver_profile.get("display") != "boot-fb" or selection_profile.get("display") != "gop-fb":
        blockers.append("display-not-gop-fb")
    if info["display_lane"] != "framebuffer":
        blockers.append("display-not-framebuffer")
    if info["input_lane"] != "ready":
        blockers.append("keyboard-not-ready")
    if info["lsys"] != "ok":
        blockers.append("lsys-not-ok")
    if info["native_pair"] != "ok":
        blockers.append("native-pair-not-ok")
    if info["progress"] not in SUCCESS_PROGRESS:
        blockers.append("shell-not-ready")
    if info["split_layer"] != "none":
        blockers.append(f"split-{info['split_layer']}")

    return ("pass", "none") if not blockers else ("blocked", ",".join(blockers))


def runtime_consequence(info: dict[str, str]) -> str:
    if info["support_cell_runtime_gate"] == "pass":
        return "continue"

    blockers = {
        blocker
        for blocker in info.get("support_cell_blockers", "").split(",")
        if blocker and blocker != "none"
    }
    split_layer = info["split_layer"]

    if split_layer in {"handoff", "storage"}:
        return "fatal-or-recovery"
    if split_layer == "driver-governance":
        return "governed-deny-or-recovery"
    if split_layer == "input":
        return "degraded-input-recovery"
    if split_layer == "display":
        return "degraded-display"

    if blockers & {
        "firmware-not-uefi",
        "storage-not-ahci-runtime",
        "storage-lane-not-ready",
        "lsys-not-ok",
        "native-pair-not-ok",
        "shell-not-ready",
    }:
        return "fatal-or-recovery"
    if "keyboard-not-ready" in blockers:
        return "degraded-input-recovery"
    if blockers & {"display-not-gop-fb", "display-not-framebuffer"}:
        return "degraded-display"
    if any(blocker.startswith("split-") for blocker in blockers):
        return "review-required"
    return "review-required"


def parse_lane(lines: list[str], storage_line: str | None, display_line: str | None, input_line: str | None, net_line: str | None) -> tuple[str, str, str, str]:
    storage_lane = "missing"
    if storage_line:
        storage_lane = "ready"
    if first_match(lines, "[BOOT] lsys read fail") or first_match(lines, "[AHCI] ") or first_match(lines, "[ATA] ") or first_match(lines, "[DISK] "):
        storage_lane = "degraded"

    display_lane = "missing"
    if display_line:
        fb_state = parse_driver(display_line, "fb")
        if fb_state == "ready":
            display_lane = "framebuffer"
        elif fb_state == "missing":
            display_lane = "text-console"
        else:
            display_lane = fb_state

    input_lane = "missing"
    if input_line:
        if "lane=ready" in input_line:
            input_lane = "ready"
        else:
            input_lane = "present"

    net_lane = "missing"
    if net_line:
        if "lane=ready" in net_line:
            net_lane = "ready"
        else:
            net_lane = "present"

    return storage_lane, display_lane, input_lane, net_lane


def detect_fwblk(lines: list[str]) -> str:
    if first_match(lines, "[BOOT] fwblk handoff ok"):
        return "ready"
    if first_match(lines, "[FWBLK] handoff missing"):
        if first_match(lines, "[BOOT] lsys super read ok"):
            return "legacy-missing-ok"
        return "missing-blocking"
    return "unknown"


def detect_progress(lines: list[str]) -> str:
    if first_match(lines, "[DESKTOP] status") or first_match(lines, "[DESKTOP] boot ok"):
        if first_match(lines, "first-setup required"):
            return "desktop-first-setup"
        if first_match(lines, "[USER] shell ready"):
            return "desktop-shell-ready"
        return "desktop"
    if first_match(lines, "[USER] shell ready"):
        if first_match(lines, "first-setup required"):
            return "shell-ready-first-setup"
        return "shell-ready"
    if first_match(lines, "[DEVICE] lane ready"):
        return "device-lane-ready"
    if first_match(lines, "[BOOT] dawn online"):
        return "boot-entered"
    return "unknown"


def is_runtime_governance_denial(line: str) -> bool:
    normalized = line.strip().lower()
    if normalized.startswith("audit ") or normalized.startswith("row "):
        return False
    return "driver.bind denied" in line or "allow_device_call denied" in line


def first_runtime_governance_denial(lines: list[str]) -> str | None:
    for line in lines:
        if is_runtime_governance_denial(line):
            return line
    return None


def detect_split_layer(lines: list[str]) -> tuple[str, str]:
    before_governance = (
        ("handoff", "[FWBLK] handoff missing"),
        ("storage", "[BOOT] lsys read fail"),
        ("storage", "[AHCI] "),
        ("storage", "[ATA] "),
        ("storage", "[DISK] "),
    )
    after_governance = (
        ("display", "[GRAPHICS] framebuffer fail"),
        ("display", "[DEVICE] display path"),
        ("input", "[VIRTKBD] pci missing"),
        ("input", "[DEVICE] input path"),
    )
    input_ready = False
    input_line = first_match(lines, "[DEVICE] input path")
    input_ctrl = first_match(lines, "[DEVICE] input ctrl ")
    if input_line and "lane=ready" in input_line:
        input_ready = True
    if input_ctrl and "legacy=i8042" in input_ctrl:
        input_ready = True
    for layer, marker in before_governance:
        line = first_match(lines, marker)
        if not line:
            continue
        if layer == "handoff" and first_match(lines, "[BOOT] lsys super read ok"):
            continue
        return layer, line
    governance_line = first_runtime_governance_denial(lines)
    if governance_line:
        return "driver-governance", governance_line
    for layer, marker in after_governance:
        line = first_match(lines, marker)
        if not line:
            continue
        if layer == "display" and first_match(lines, "[USER] shell ready"):
            continue
        if marker == "[VIRTKBD] pci missing" and input_ready:
            continue
        if layer == "input" and line.startswith("[DEVICE] input path") and "lane=ready" in line:
            continue
        return layer, line
    return "none", "(none)"


def detect_governance(lines: list[str]) -> str:
    return first_runtime_governance_denial(lines) or "(none)"


def detect_residual(lines: list[str]) -> str:
    for line in lines:
        if line.startswith("[DISK] ") or line.startswith("[AHCI] ") or line.startswith("[ATA] "):
            return line
    return "(none)"


def parse_layout_start(line: str | None, key: str) -> int | None:
    if not line:
        return None
    match = re.search(rf"{re.escape(key)}=([0-9A-Fa-f]+)", line)
    if not match:
        return None
    return int(match.group(1), 16)


def parse_residual_lba(line: str) -> int | None:
    match = re.search(r"lba=([0-9A-Fa-f]+)", line)
    if not match:
        return None
    return int(match.group(1), 16)


def classify_residual_region(layout_line: str | None, residual_line: str) -> str:
    if residual_line == "(none)":
        return "(none)"
    residual_lba = parse_residual_lba(residual_line)
    if residual_lba is None:
        return "unknown"
    lsys_lba = parse_layout_start(layout_line, "lsys")
    ldat_lba = parse_layout_start(layout_line, "ldat")
    if lsys_lba is not None and residual_lba >= lsys_lba:
        if ldat_lba is None or residual_lba < ldat_lba:
            return f"lsys+0x{residual_lba - lsys_lba:04X}"
    if ldat_lba is not None and residual_lba >= ldat_lba:
        return f"ldat+0x{residual_lba - ldat_lba:04X}"
    return f"lba=0x{residual_lba:08X}"


def classify(log_path: Path) -> dict[str, str]:
    lines = read_lines(log_path)
    firmware = detect_firmware(lines)
    fwblk = detect_fwblk(lines)
    progress = detect_progress(lines)

    storage_line = first_match(lines, "[DEVICE] disk path")
    storage_select_line = first_match(lines, "[DEVICE] disk select ")
    layout_line = first_match(lines, "[DEVICE] disk layout ")
    storage_pci = first_match(lines, "[DEVICE] disk pci ")
    display_line = first_match(lines, "[DEVICE] display path")
    display_select_line = first_match(lines, "[DEVICE] display select ")
    display_pci = first_match(lines, "[DEVICE] display pci ")
    input_line = first_match(lines, "[DEVICE] input path")
    input_select_line = first_match(lines, "[DEVICE] input select ")
    input_pci = first_match(lines, "[DEVICE] input pci ")
    input_ctrl = first_match(lines, "[DEVICE] input ctrl ")
    input_usb_candidate = first_match(lines, "[DEVICE] input usb candidate")
    input_usb_pci = first_match(lines, "[DEVICE] input usb pci ")
    serial_line = first_match(lines, "[DEVICE] serial path")
    serial_select_line = first_match(lines, "[DEVICE] serial select ")
    serial_pci = first_match(lines, "[DEVICE] serial pci ")
    net_line = first_match(lines, "[DEVICE] net path")
    net_select_line = first_match(lines, "[DEVICE] net select ")
    net_pci = first_match(lines, "[DEVICE] net pci ")
    platform_pci = first_match(lines, "[DEVICE] platform pci ")

    storage_lane, display_lane, input_lane, net_lane = parse_lane(
        lines, storage_line, display_line, input_line, net_line
    )
    split_layer, split_evidence = detect_split_layer(lines)
    residual = detect_residual(lines)
    lsys_start = parse_layout_start(layout_line, "lsys")
    ldat_start = parse_layout_start(layout_line, "ldat")
    residual_region = classify_residual_region(layout_line, residual)
    governance = detect_governance(lines)

    lsys_status = "ok" if first_match(lines, "[BOOT] lsys super read ok") else "fail" if first_match(lines, "[BOOT] lsys read fail") else "unknown"
    native_pair = "ok" if first_match(lines, "[BOOT] native pair ok") else "missing"

    driver_profile = (
        build_profile(
            ("storage", parse_driver(storage_line, "driver")),
            ("serial", parse_driver(serial_line, "driver")),
            ("display", parse_driver(display_line, "driver")),
            ("input", parse_driver(input_line, "kbd")),
            ("net", parse_driver(net_line, "driver")),
        )
    )
    selection_profile = (
        build_profile(
            ("storage", parse_driver(storage_select_line, "basis")),
            ("serial", parse_driver(serial_select_line, "basis")),
            ("display", parse_driver(display_select_line, "basis")),
            ("input", parse_driver(input_select_line, "basis")),
            ("net", parse_driver(net_select_line, "basis")),
        )
    )

    info = {
        "log": str(log_path),
        "firmware": firmware,
        "driver_profile": driver_profile,
        "selection_profile": selection_profile,
        "layout_lsys": f"0x{lsys_start:08X}" if lsys_start is not None else "(missing)",
        "layout_ldat": f"0x{ldat_start:08X}" if ldat_start is not None else "(missing)",
        "storage_pci": storage_pci or "(missing)",
        "serial_pci": serial_pci or "(missing)",
        "display_pci": display_pci or "(missing)",
        "input_identity": input_identity(input_pci, input_ctrl, input_usb_candidate, input_usb_pci),
        "net_pci": net_pci or "(missing)",
        "platform_pci": platform_pci or "(missing)",
        "fwblk": fwblk,
        "lsys": lsys_status,
        "native_pair": native_pair,
        "storage_lane": storage_lane,
        "display_lane": display_lane,
        "input_lane": input_lane,
        "net_lane": net_lane,
        "progress": progress,
        "split_layer": split_layer,
        "split_evidence": split_evidence,
        "storage_residual": residual,
        "storage_residual_region": residual_region,
        "driver_governance": governance,
    }
    gate, blockers = support_cell_runtime_gate(info)
    info["target_support_cell"] = TARGET_SUPPORT_CELL
    info["support_cell_runtime_gate"] = gate
    info["support_cell_blockers"] = blockers
    info["runtime_consequence"] = runtime_consequence(info)
    return info


def summarize(log_path: Path) -> str:
    info = classify(log_path)
    out = [
        f"log: {info['log']}",
        "",
        "[identity]",
        f"firmware={info['firmware']}",
        f"driver_profile={info['driver_profile']}",
        f"selection_profile={info['selection_profile']}",
        f"layout_lsys={info['layout_lsys']}",
        f"layout_ldat={info['layout_ldat']}",
        f"storage_pci={info['storage_pci']}",
        f"serial_pci={info['serial_pci']}",
        f"display_pci={info['display_pci']}",
        f"input_identity={info['input_identity']}",
        f"net_pci={info['net_pci']}",
        f"platform_pci={info['platform_pci']}",
        "",
        "[contracts]",
        f"fwblk={info['fwblk']}",
        f"lsys={info['lsys']}",
        f"native_pair={info['native_pair']}",
        f"storage_lane={info['storage_lane']}",
        f"display_lane={info['display_lane']}",
        f"input_lane={info['input_lane']}",
        f"net_lane={info['net_lane']}",
        f"progress={info['progress']}",
        "",
        "[failure]",
        f"split_layer={info['split_layer']}",
        f"split_evidence={info['split_evidence']}",
        f"storage_residual={info['storage_residual']}",
        f"storage_residual_region={info['storage_residual_region']}",
        f"driver_governance={info['driver_governance']}",
        "",
        "[triage]",
        "first_real_machine_focus=handoff,storage,input,display,driver-family",
        f"target_support_cell={info['target_support_cell']}",
        f"support_cell_runtime_gate={info['support_cell_runtime_gate']}",
        f"support_cell_blockers={info['support_cell_blockers']}",
        f"runtime_consequence={info['runtime_consequence']}",
        "",
    ]
    return "\n".join(out)


def main() -> int:
    try:
        log_path = Path(sys.argv[1]).resolve()
        if not log_path.exists():
            raise FileNotFoundError(log_path)
        sys.stdout.write(summarize(log_path))
        return 0
    except Exception as exc:
        sys.stderr.write(f"firsthop classification failed: {exc}\n")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
