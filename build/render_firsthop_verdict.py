from __future__ import annotations

import hashlib
import sys
from pathlib import Path

from classify_firsthop import classify, classify_residual_region
from compare_firsthop import compare_data
from select_firsthop_baseline import select_baseline


EVIDENCE_SCOPE_VIRTUALIZED = "virtualized-prephysical"
EVIDENCE_SCOPE_PHYSICAL = "physical-candidate"
EVIDENCE_SCOPE_UNKNOWN = "unknown"
TARGET_SUPPORT_CELL = "intel-x86_64+uefi+sata-ahci+gop+keyboard"
EVIDENCE_MANIFEST_NAME = "evidence-manifest.txt"
EVIDENCE_MANIFEST_SCHEMA = "physical-evidence-v1"
REQUIRED_PHYSICAL_MANIFEST_KEYS = (
    "schema",
    "target_support_cell",
    "evidence_scope",
    "capture_log",
    "capture_log_sha256",
    "operator_notes",
    "operator_notes_sha256",
    "machine",
    "machine_sha256",
)

VIRTUALIZED_LOG_MARKERS = ("qemu", "vmware")
PHYSICAL_SESSION_MARKER = "/artifacts/physical_bringup/"
PLACEHOLDER_NOTE_LINES = {
    "paste run notes here.",
    "include: machine model, firmware version, sata mode, usb port used, last visible line, shell-ready result.",
    "replace every value before finalization.",
}
REQUIRED_PHYSICAL_NOTE_KEYS = (
    "machine_model",
    "firmware_version",
    "sata_mode",
    "usb_port",
    "capture_source",
    "last_visible_line",
    "shell_ready",
    "gop_result",
    "keyboard_result",
)
PHYSICAL_NOTE_ENUMS = {
    "capture_source": {"serial", "display-photo", "operator-transcript"},
    "shell_ready": {"yes", "no", "not-reached"},
    "gop_result": {"ready", "missing", "not-reached"},
    "keyboard_result": {"ready", "blocked", "not-reached"},
}


def is_virtualized_log_path(log_path: Path) -> bool:
    name = log_path.name.lower()
    path_text = str(log_path).lower().replace("\\", "/")
    return any(marker in name for marker in VIRTUALIZED_LOG_MARKERS) or "/artifacts/vmware_bringup/" in path_text


def in_physical_session(log_path: Path) -> bool:
    path_text = str(log_path).lower().replace("\\", "/")
    return PHYSICAL_SESSION_MARKER in path_text


def read_optional_text(path: Path) -> str | None:
    try:
        if path.exists():
            return path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None
    return None


def parse_key_values(text: str | None) -> dict[str, str]:
    values: dict[str, str] = {}
    if text is None:
        return values
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key.strip()] = value.strip()
    return values


def blocker_key(key: str) -> str:
    return key.replace("_", "-")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def manifest_file_blockers(
    session_dir: Path,
    values: dict[str, str],
    filename_key: str,
    hash_key: str,
    expected_name: str,
) -> list[str]:
    blockers: list[str] = []
    filename = values.get(filename_key, "").strip()
    expected_blocker = blocker_key(filename_key)
    if not filename:
        return [f"evidence-manifest-{expected_blocker}-missing"]
    if filename != expected_name:
        blockers.append(f"evidence-manifest-{expected_blocker}-mismatch")
    file_path = session_dir / filename
    if filename != Path(filename).name:
        blockers.append(f"evidence-manifest-{expected_blocker}-path-invalid")
        return blockers
    if not file_path.exists() or not file_path.is_file():
        blockers.append(f"evidence-manifest-{expected_blocker}-file-missing")
        return blockers
    expected_hash = values.get(hash_key, "").strip().lower()
    if not expected_hash:
        blockers.append(f"evidence-manifest-{blocker_key(hash_key)}-missing")
    else:
        try:
            actual_hash = sha256_file(file_path)
        except OSError:
            actual_hash = ""
        if actual_hash != expected_hash:
            blockers.append(f"evidence-manifest-{blocker_key(hash_key)}-mismatch")
    return blockers


def physical_manifest_blockers(session_dir: Path, log_path: Path) -> list[str]:
    manifest_text = read_optional_text(session_dir / EVIDENCE_MANIFEST_NAME)
    if manifest_text is None:
        return ["evidence-manifest-missing"]

    values = parse_key_values(manifest_text)
    blockers: list[str] = []
    for key in REQUIRED_PHYSICAL_MANIFEST_KEYS:
        if not values.get(key, "").strip():
            blockers.append(f"evidence-manifest-{blocker_key(key)}-missing")

    if values.get("schema") and values["schema"] != EVIDENCE_MANIFEST_SCHEMA:
        blockers.append("evidence-manifest-schema-invalid")
    if values.get("target_support_cell") and values["target_support_cell"] != TARGET_SUPPORT_CELL:
        blockers.append("evidence-manifest-target-support-cell-mismatch")
    if values.get("evidence_scope") and values["evidence_scope"] != EVIDENCE_SCOPE_PHYSICAL:
        blockers.append("evidence-manifest-evidence-scope-mismatch")

    blockers.extend(
        manifest_file_blockers(
            session_dir,
            values,
            "capture_log",
            "capture_log_sha256",
            log_path.name,
        )
    )
    blockers.extend(
        manifest_file_blockers(
            session_dir,
            values,
            "operator_notes",
            "operator_notes_sha256",
            "operator-notes.txt",
        )
    )
    blockers.extend(
        manifest_file_blockers(
            session_dir,
            values,
            "machine",
            "machine_sha256",
            "machine.txt",
        )
    )
    return list(dict.fromkeys(blockers))


def physical_note_blockers(session_dir: Path) -> list[str]:
    notes = read_optional_text(session_dir / "operator-notes.txt")
    if notes is None:
        return ["operator-notes-missing"]

    values = parse_key_values(notes)
    blockers: list[str] = []
    for raw_line in notes.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        if line.lower() in PLACEHOLDER_NOTE_LINES:
            blockers.append("operator-notes-placeholder")
            continue

    for key in REQUIRED_PHYSICAL_NOTE_KEYS:
        value = values.get(key, "").strip()
        if not value:
            blockers.append(f"operator-note-{blocker_key(key)}-missing")

    for key, allowed_values in PHYSICAL_NOTE_ENUMS.items():
        value = values.get(key, "").strip().lower()
        if value and value not in allowed_values:
            blockers.append(f"operator-note-{blocker_key(key)}-invalid")

    return list(dict.fromkeys(blockers))


def physical_evidence_chain(log_path: Path) -> dict[str, str]:
    session_dir = log_path.parent
    blockers: list[str] = []

    if not in_physical_session(log_path):
        return {
            "status": "not-applicable",
            "blockers": "not-applicable",
        }

    if is_virtualized_log_path(log_path):
        blockers.append("virtualized-log-name")

    machine_text = read_optional_text(session_dir / "machine.txt")
    machine_values = parse_key_values(machine_text)
    if machine_text is None:
        blockers.append("machine-metadata-missing")
    else:
        if machine_values.get("target_support_cell") != TARGET_SUPPORT_CELL:
            blockers.append("target-cell-metadata-missing")
        if machine_values.get("evidence_scope") != EVIDENCE_SCOPE_PHYSICAL:
            blockers.append("physical-scope-metadata-missing")

    blockers.extend(physical_note_blockers(session_dir))
    blockers.extend(physical_manifest_blockers(session_dir, log_path))

    return {
        "status": "present" if not blockers else "missing",
        "blockers": "none" if not blockers else ",".join(blockers),
    }


def detect_evidence_scope(log_path: Path, physical_evidence: dict[str, str] | None = None) -> str:
    if is_virtualized_log_path(log_path):
        return EVIDENCE_SCOPE_VIRTUALIZED
    if physical_evidence is None:
        physical_evidence = physical_evidence_chain(log_path)
    if physical_evidence["status"] == "present":
        return EVIDENCE_SCOPE_PHYSICAL
    return EVIDENCE_SCOPE_UNKNOWN


def normalize_evidence_scope(
    log_path: Path,
    evidence_scope: str = "auto",
    physical_evidence: dict[str, str] | None = None,
) -> str:
    if physical_evidence is None:
        physical_evidence = physical_evidence_chain(log_path)
    if evidence_scope == "auto":
        return detect_evidence_scope(log_path, physical_evidence)
    if evidence_scope in {EVIDENCE_SCOPE_VIRTUALIZED, EVIDENCE_SCOPE_PHYSICAL, EVIDENCE_SCOPE_UNKNOWN}:
        if evidence_scope == EVIDENCE_SCOPE_PHYSICAL and physical_evidence["status"] != "present":
            return EVIDENCE_SCOPE_UNKNOWN
        return evidence_scope
    raise ValueError(f"unknown evidence scope: {evidence_scope}")


def support_cell_status(classification: dict[str, str], evidence_scope: str) -> str:
    if evidence_scope == EVIDENCE_SCOPE_VIRTUALIZED:
        return "not-established-prephysical-only"
    if evidence_scope == EVIDENCE_SCOPE_PHYSICAL:
        if classification["support_cell_runtime_gate"] == "pass":
            return "not-established-physical-candidate-needs-review"
        return "not-established-physical-blocked"
    return "not-established-evidence-scope-unknown"


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


def next_action(
    classification: dict[str, str],
    selection: dict[str, object],
    compare_info: dict[str, object] | None,
    evidence_scope: str,
    physical_evidence: dict[str, str],
) -> str:
    if evidence_scope == EVIDENCE_SCOPE_UNKNOWN and physical_evidence["status"] == "missing":
        return "complete physical session evidence chain before treating this as real-machine support evidence"

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
        if evidence_scope == EVIDENCE_SCOPE_VIRTUALIZED:
            return "keep this as pre-physical evidence and capture a real-machine serial log before moving the support cell"
        if evidence_scope == EVIDENCE_SCOPE_PHYSICAL:
            return "review physical artifacts and operator notes before moving the support cell from not established"
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


def render_verdict(log_path: Path, evidence_scope: str = "auto") -> str:
    classification = classify(log_path)
    selection = select_baseline(log_path)
    physical_evidence = physical_evidence_chain(log_path)
    resolved_evidence_scope = normalize_evidence_scope(log_path, evidence_scope, physical_evidence)

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
        f"target_support_cell={classification['target_support_cell']}",
        f"evidence_scope={resolved_evidence_scope}",
        f"physical_evidence_status={physical_evidence['status']}",
        f"physical_evidence_blockers={physical_evidence['blockers']}",
        f"support_cell_runtime_gate={classification['support_cell_runtime_gate']}",
        f"support_cell_blockers={classification['support_cell_blockers']}",
        f"runtime_consequence={classification['runtime_consequence']}",
        f"support_cell_status={support_cell_status(classification, resolved_evidence_scope)}",
        f"storage_residual={classification['storage_residual']}",
        f"storage_residual_region={residual_region}",
        f"next_action={next_action(classification, selection, compare_info, resolved_evidence_scope, physical_evidence)}",
        "",
    ]
    return "\n".join(out)


def main() -> int:
    try:
        args = sys.argv[1:]
        evidence_scope = "auto"
        if len(args) >= 2 and args[0] == "--evidence-scope":
            evidence_scope = args[1]
            args = args[2:]
        if len(args) != 1:
            raise RuntimeError(
                "usage: render_firsthop_verdict.py [--evidence-scope auto|physical-candidate|virtualized-prephysical|unknown] <log>"
            )
        log_path = Path(args[0]).resolve()
        if not log_path.exists():
            raise FileNotFoundError(log_path)
        sys.stdout.write(render_verdict(log_path, evidence_scope=evidence_scope))
        return 0
    except Exception as exc:
        sys.stderr.write(f"firsthop verdict render failed: {exc}\n")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
