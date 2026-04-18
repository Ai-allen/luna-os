from __future__ import annotations

import sys
from pathlib import Path

from classify_firsthop import classify
from compare_firsthop import compare_data


ROOT = Path(__file__).resolve().parent
SUCCESS_PROGRESS = {
    "shell-ready",
    "shell-ready-first-setup",
    "desktop-shell-ready",
    "desktop-first-setup",
}
CANDIDATES = (
    ROOT / "qemu_bootcheck.log",
    ROOT / "qemu_uefi_shellcheck.log",
    ROOT / "vmware_desktopcheck.serial.log",
    ROOT / "vmware_m1.serial.log",
)


def is_missing(value: str) -> bool:
    return value in {"missing", "unknown", "(missing)"}


def is_healthy(info: dict[str, str]) -> bool:
    return (
        info["split_layer"] == "none"
        and info["lsys"] == "ok"
        and info["native_pair"] == "ok"
        and info["progress"] in SUCCESS_PROGRESS
    )


def completeness_score(info: dict[str, str]) -> int:
    score = 0
    for key in (
        "firmware",
        "driver_profile",
        "storage_pci",
        "serial_pci",
        "display_pci",
        "input_identity",
        "net_pci",
        "platform_pci",
    ):
        if not is_missing(info[key]):
            score += 1
    for key in ("storage_lane", "display_lane", "input_lane", "net_lane"):
        if info[key] not in {"missing", "unknown"}:
            score += 1
    return score


def delta_count(delta_layers: str) -> int:
    if delta_layers == "none":
        return 0
    return len([layer for layer in delta_layers.split(",") if layer])


def firmware_match_rank(actual_firmware: str, candidate_firmware: str) -> int:
    if actual_firmware == "unknown":
        return 1
    if actual_firmware == candidate_firmware:
        return 2
    return 0


def select_baseline(actual_path: Path) -> dict[str, object]:
    actual_info = classify(actual_path)
    actual_firmware = actual_info["firmware"]
    scored: list[tuple[tuple[int, int, int, int], dict[str, object]]] = []

    for candidate_path in CANDIDATES:
        if not candidate_path.exists():
            continue
        candidate_info = classify(candidate_path)
        compare_info = compare_data(candidate_path, actual_path)
        healthy = is_healthy(candidate_info)
        score = (
            1 if healthy else 0,
            firmware_match_rank(actual_firmware, candidate_info["firmware"]),
            -delta_count(str(compare_info["delta_layers"])),
            completeness_score(candidate_info),
        )
        scored.append(
            (
                score,
                {
                    "score": score,
                    "path": candidate_path,
                    "name": candidate_path.name,
                    "candidate": candidate_info,
                    "compare": compare_info,
                    "healthy": healthy,
                },
            )
        )

    if not scored:
        return {
            "status": "baseline-unavailable",
            "actual_firmware": actual_firmware,
            "selected": None,
        }

    scored.sort(key=lambda item: item[0], reverse=True)
    _, selected = scored[0]
    selected_candidate = selected["candidate"]
    selected_compare = selected["compare"]
    candidates_rendered: list[str] = []
    for _, candidate_item in scored:
        candidate_info = candidate_item["candidate"]
        compare_info = candidate_item["compare"]
        score = candidate_item["score"]
        candidates_rendered.append(
            "|".join(
                (
                    candidate_item["name"],
                    f"healthy={'yes' if candidate_item['healthy'] else 'no'}",
                    f"firmware={candidate_info['firmware']}",
                    f"score={score[0]}/{score[1]}/{score[2]}/{score[3]}",
                    f"delta_layers={compare_info['delta_layers']}",
                    f"priority_blocker={compare_info['priority_blocker']}",
                    f"driver_family_delta={compare_info['driver_family_delta']}",
                )
            )
        )
    return {
        "status": "selected",
        "actual_firmware": actual_firmware,
        "selected": selected["name"],
        "selected_path": str(selected["path"]),
        "selected_firmware": selected_candidate["firmware"],
        "selected_healthy": "yes" if selected["healthy"] else "no",
        "selected_completeness": str(completeness_score(selected_candidate)),
        "selected_delta_layers": str(selected_compare["delta_layers"]),
        "selected_priority_blocker": str(selected_compare["priority_blocker"]),
        "selected_driver_family_delta": str(selected_compare["driver_family_delta"]),
        "candidate_lines": candidates_rendered,
    }


def render_selection(info: dict[str, object]) -> str:
    out = [
        f"status={info['status']}",
        f"actual_firmware={info['actual_firmware']}",
    ]
    if info["status"] == "selected":
        out.extend(
            (
                f"selected={info['selected']}",
                f"selected_path={info['selected_path']}",
                f"selected_firmware={info['selected_firmware']}",
                f"selected_healthy={info['selected_healthy']}",
                f"selected_completeness={info['selected_completeness']}",
                f"selected_delta_layers={info['selected_delta_layers']}",
                f"selected_priority_blocker={info['selected_priority_blocker']}",
                f"selected_driver_family_delta={info['selected_driver_family_delta']}",
            )
        )
        for line in info.get("candidate_lines", []):
            out.append(f"candidate={line}")
    return "\n".join(out) + "\n"


def main() -> int:
    try:
        if len(sys.argv) != 2:
            raise RuntimeError("usage: select_firsthop_baseline.py <actual-log>")
        actual_path = Path(sys.argv[1]).resolve()
        if not actual_path.exists():
            raise FileNotFoundError(actual_path)
        sys.stdout.write(render_selection(select_baseline(actual_path)))
        return 0
    except Exception as exc:
        sys.stderr.write(f"firsthop baseline select failed: {exc}\n")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
