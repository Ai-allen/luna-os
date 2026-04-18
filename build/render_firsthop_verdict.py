from __future__ import annotations

import sys
from pathlib import Path

from classify_firsthop import classify, classify_residual_region
from compare_firsthop import compare_data
from select_firsthop_baseline import select_baseline


def refine_residual_region(classification: dict[str, str], compare_info: dict[str, object] | None) -> str:
    region = classification["storage_residual_region"]
    if region != "(none)" and not region.startswith("lba="):
        return region
    if compare_info is None:
        return region
    reference = compare_info["reference"]
    if reference["layout_lsys"] == "(missing)" and reference["layout_ldat"] == "(missing)":
        return region
    layout_line = f"[DEVICE] disk layout lsys={reference['layout_lsys'].replace('0x', '')} ldat={reference['layout_ldat'].replace('0x', '')}"
    refined = classify_residual_region(layout_line, classification["storage_residual"])
    if refined == region:
        return region
    return f"{refined} via {Path(str(compare_info['reference_path'])).name}"


def next_action(classification: dict[str, str], selection: dict[str, object], compare_info: dict[str, object] | None) -> str:
    split_layer = classification["split_layer"]
    if split_layer == "handoff":
        return "check loader handoff, firmware block source/full-disk contract, and early BOOT handoff state"
    if split_layer == "storage":
        return "check lsys/native pair read path, storage driver family, and low-level disk error evidence"
    if split_layer == "input":
        return "check input driver family, lane readiness, and legacy vs virtio keyboard path"
    if split_layer == "display":
        return "check GOP/framebuffer readiness, display driver family, and console fallback"
    if split_layer == "driver-governance":
        return "check SECURITY/DEVICE governance denial, driver bind approval, and lifecycle gate state"

    if compare_info is None:
        return "capture more runtime evidence; no healthy virtualized baseline was available"

    priority_blocker = str(compare_info["priority_blocker"])
    if priority_blocker == "none":
        return "capture the first real-machine log and compare it against the selected healthy baseline"
    if priority_blocker == "driver-family":
        return "compare pci identity, driver-family, and selection-basis delta against the selected healthy baseline"
    if priority_blocker == "input":
        return "compare input lane and keyboard family against the selected healthy baseline"
    if priority_blocker == "display":
        return "compare framebuffer/display lane against the selected healthy baseline"
    if priority_blocker == "storage":
        return "compare storage lane, lsys/native pair state, and residual disk evidence against the selected healthy baseline"
    if priority_blocker == "handoff":
        return "compare firmware handoff state against the selected healthy baseline"
    if priority_blocker == "driver-governance":
        return "compare governance denials and driver-bind approvals against the selected healthy baseline"
    if priority_blocker == "progress":
        return "compare bring-up progression and first user-visible stop point against the selected healthy baseline"
    return "continue layer-by-layer comparison from handoff to storage, input, display, and driver-family"


def render_verdict(log_path: Path) -> str:
    classification = classify(log_path)
    selection = select_baseline(log_path)

    compare_info = None
    if selection["status"] == "selected" and selection.get("selected_path"):
        compare_info = compare_data(Path(str(selection["selected_path"])), log_path)

    reference_name = str(selection.get("selected") or "(missing)")
    reference_health = str(selection.get("selected_healthy") or "unknown")
    delta_layers = "unknown"
    priority_blocker = classification["split_layer"]
    driver_family_delta = "unknown"
    if compare_info is not None:
        delta_layers = str(compare_info["delta_layers"])
        priority_blocker = str(compare_info["priority_blocker"])
        driver_family_delta = str(compare_info["driver_family_delta"])
    residual_region = refine_residual_region(classification, compare_info)

    out = [
        f"log={log_path}",
        "",
        "[verdict]",
        f"firmware={classification['firmware']}",
        f"progress={classification['progress']}",
        f"split_layer={classification['split_layer']}",
        f"priority_blocker={priority_blocker}",
        f"reference={reference_name}",
        f"reference_healthy={reference_health}",
        f"delta_layers={delta_layers}",
        f"driver_family_delta={driver_family_delta}",
        f"storage_residual={classification['storage_residual']}",
        f"storage_residual_region={residual_region}",
        f"next_action={next_action(classification, selection, compare_info)}",
        "",
    ]
    return "\n".join(out)


def main() -> int:
    try:
        if len(sys.argv) != 2:
            raise RuntimeError("usage: render_firsthop_verdict.py <log>")
        log_path = Path(sys.argv[1]).resolve()
        if not log_path.exists():
            raise FileNotFoundError(log_path)
        sys.stdout.write(render_verdict(log_path))
        return 0
    except Exception as exc:
        sys.stderr.write(f"firsthop verdict render failed: {exc}\n")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
