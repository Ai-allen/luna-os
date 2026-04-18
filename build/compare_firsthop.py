from __future__ import annotations

import sys
from pathlib import Path

from classify_firsthop import classify


IDENTITY_KEYS = (
    "firmware",
    "driver_profile",
    "selection_profile",
    "layout_lsys",
    "layout_ldat",
    "storage_pci",
    "serial_pci",
    "display_pci",
    "input_identity",
    "net_pci",
    "platform_pci",
)

CONTRACT_KEYS = (
    "fwblk",
    "lsys",
    "native_pair",
    "storage_lane",
    "display_lane",
    "input_lane",
    "net_lane",
    "progress",
)

FAILURE_KEYS = (
    "split_layer",
    "split_evidence",
    "storage_residual",
    "storage_residual_region",
    "driver_governance",
)


def parse_driver_profile(profile: str) -> dict[str, str]:
    out: dict[str, str] = {}
    for part in profile.split(","):
        if "=" not in part:
            continue
        key, value = part.split("=", 1)
        out[key] = value
    return out


def driver_family_delta(reference: dict[str, str], actual: dict[str, str]) -> str:
    ref = parse_driver_profile(reference["driver_profile"])
    cur = parse_driver_profile(actual["driver_profile"])
    ref_basis = parse_driver_profile(reference.get("selection_profile", ""))
    cur_basis = parse_driver_profile(actual.get("selection_profile", ""))
    keys = ("storage", "serial", "display", "input", "net")
    diffs: list[str] = []
    for key in keys:
        ref_value = ref.get(key, "missing")
        cur_value = cur.get(key, "missing")
        ref_select = ref_basis.get(key, "missing")
        cur_select = cur_basis.get(key, "missing")
        if ref_value != cur_value or ref_select != cur_select:
            diffs.append(f"{key}:{ref_value}/{ref_select}->{cur_value}/{cur_select}")
    return ",".join(diffs) if diffs else "none"


def changed_layers(reference: dict[str, str], actual: dict[str, str]) -> str:
    layers: list[str] = []

    def add(layer: str) -> None:
        if layer not in layers:
            layers.append(layer)

    if reference["fwblk"] != actual["fwblk"]:
        add("handoff")
    if any(reference[key] != actual[key] for key in ("lsys", "native_pair", "storage_lane", "storage_residual")):
        add("storage")
    if reference["display_lane"] != actual["display_lane"]:
        add("display")
    if reference["input_lane"] != actual["input_lane"]:
        add("input")
    if reference["driver_governance"] != actual["driver_governance"]:
        add("driver-governance")
    if any(reference[key] != actual[key] for key in ("driver_profile", "selection_profile", "storage_pci", "serial_pci", "display_pci", "input_identity", "net_pci", "platform_pci")):
        add("driver-family")
    if reference["progress"] != actual["progress"]:
        add("progress")

    return ",".join(layers) if layers else "none"


def choose_priority_blocker(reference: dict[str, str], actual: dict[str, str], delta_layers: str) -> str:
    if actual["split_layer"] != "none":
        return actual["split_layer"]

    ordered = (
        ("handoff", reference["fwblk"] != actual["fwblk"]),
        (
            "storage",
            any(
                reference[key] != actual[key]
                for key in ("lsys", "native_pair", "storage_lane", "storage_residual")
            ),
        ),
        ("input", reference["input_lane"] != actual["input_lane"]),
        ("display", reference["display_lane"] != actual["display_lane"]),
        ("driver-governance", reference["driver_governance"] != actual["driver_governance"]),
        (
            "driver-family",
            any(
                reference[key] != actual[key]
                for key in (
                    "driver_profile",
                    "selection_profile",
                    "storage_pci",
                    "serial_pci",
                    "display_pci",
                    "input_identity",
                    "net_pci",
                    "platform_pci",
                )
            ),
        ),
        ("progress", reference["progress"] != actual["progress"]),
    )
    for layer, changed in ordered:
        if changed:
            return layer
    if delta_layers != "none":
        return delta_layers.split(",", 1)[0]
    return "none"


def format_delta(label: str, keys: tuple[str, ...], reference: dict[str, str], actual: dict[str, str]) -> list[str]:
    out = [f"[{label}]"]
    changed = False
    for key in keys:
        if reference[key] != actual[key]:
            out.append(f"{key}: {reference[key]} -> {actual[key]}")
            changed = True
    if not changed:
        out.append("(aligned)")
    out.append("")
    return out


def compare_data(reference_path: Path, actual_path: Path) -> dict[str, object]:
    reference = classify(reference_path)
    actual = classify(actual_path)
    delta_layers = changed_layers(reference, actual)
    status = "aligned" if delta_layers == "none" else "diverged"
    priority_blocker = choose_priority_blocker(reference, actual, delta_layers)
    driver_delta = driver_family_delta(reference, actual)
    return {
        "reference_path": reference_path,
        "actual_path": actual_path,
        "reference": reference,
        "actual": actual,
        "status": status,
        "delta_layers": delta_layers,
        "priority_blocker": priority_blocker,
        "driver_family_delta": driver_delta,
    }


def compare(reference_path: Path, actual_path: Path) -> str:
    info = compare_data(reference_path, actual_path)
    reference = info["reference"]
    actual = info["actual"]
    out = [
        f"reference: {info['reference_path']}",
        f"actual: {info['actual_path']}",
        "",
        "[summary]",
        f"status={info['status']}",
        f"delta_layers={info['delta_layers']}",
        f"priority_blocker={info['priority_blocker']}",
        f"driver_family_delta={info['driver_family_delta']}",
        f"reference_progress={reference['progress']}",
        f"actual_progress={actual['progress']}",
        "",
    ]
    out.extend(format_delta("identity", IDENTITY_KEYS, reference, actual))
    out.extend(format_delta("contracts", CONTRACT_KEYS, reference, actual))
    out.extend(format_delta("failure", FAILURE_KEYS, reference, actual))
    return "\n".join(out)


def main() -> int:
    try:
        if len(sys.argv) != 3:
            raise RuntimeError("usage: compare_firsthop.py <reference-log> <actual-log>")
        reference_path = Path(sys.argv[1]).resolve()
        actual_path = Path(sys.argv[2]).resolve()
        if not reference_path.exists():
            raise FileNotFoundError(reference_path)
        if not actual_path.exists():
            raise FileNotFoundError(actual_path)
        sys.stdout.write(compare(reference_path, actual_path))
        return 0
    except Exception as exc:
        sys.stderr.write(f"firsthop compare failed: {exc}\n")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
