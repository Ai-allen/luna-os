from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent
REPO_ROOT = ROOT.parent
ARTIFACTS_ROOT = REPO_ROOT / "artifacts"


def find_pwsh() -> str:
    found = shutil.which("pwsh")
    if found:
        return found
    for candidate in (
        Path(r"C:\Program Files\PowerShell\7\pwsh.exe"),
        Path(r"C:\Program Files\PowerShell\7-preview\pwsh.exe"),
    ):
        if candidate.exists():
            return str(candidate)
    raise FileNotFoundError("pwsh not found")


def run_ps(pwsh: str, script: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [pwsh, "-NoProfile", "-File", str(script), *args],
        cwd=REPO_ROOT,
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )


def extract_session_path(stdout: str) -> Path:
    for line in stdout.splitlines():
        line = line.strip()
        if line.startswith("Physical bring-up session prepared: "):
            return Path(line.split(": ", 1)[1])
        if line.startswith("VMware bring-up session prepared: "):
            return Path(line.split(": ", 1)[1])
    raise RuntimeError(f"session path missing in output:\n{stdout}")


def assert_summary(
    session_dir: Path,
    log_path: Path,
    expected_reference: str,
    expected_firmware: str,
    expected_progress: str,
    expected_evidence_scope: str,
    expected_physical_status: str,
    expected_support_cell_status: str,
    expected_physical_blocker: str | None = None,
    expected_runtime_consequence: str | None = None,
    expected_delta_status: str = "aligned",
    expected_delta_layers: str = "none",
    expected_priority_blocker: str = "none",
    expected_driver_family_delta: str = "none",
    expected_selected_delta_layers: str | None = None,
    expected_selected_priority_blocker: str | None = None,
    expected_selected_driver_family_delta: str | None = None,
) -> None:
    finalize_script = session_dir / "finalize-session.ps1"
    if not finalize_script.exists():
        raise RuntimeError(f"missing finalize script: {finalize_script}")

    pwsh = find_pwsh()
    run_ps(pwsh, finalize_script, "-LogPath", str(log_path))

    summary_path = session_dir / "firsthop-summary.txt"
    classification_path = session_dir / "firsthop-classification.txt"
    reference_path = session_dir / "firsthop-reference.txt"
    delta_path = session_dir / "firsthop-delta.txt"
    verdict_path = session_dir / "firsthop-verdict.txt"
    meta_path = session_dir / "firsthop-log.txt"
    manifest_path = session_dir / "evidence-manifest.txt"
    if not summary_path.exists():
        raise RuntimeError(f"missing firsthop summary: {summary_path}")
    if not classification_path.exists():
        raise RuntimeError(f"missing firsthop classification: {classification_path}")
    if not reference_path.exists():
        raise RuntimeError(f"missing firsthop reference: {reference_path}")
    if not delta_path.exists():
        raise RuntimeError(f"missing firsthop delta: {delta_path}")
    if not verdict_path.exists():
        raise RuntimeError(f"missing firsthop verdict: {verdict_path}")
    if not meta_path.exists():
        raise RuntimeError(f"missing firsthop log metadata: {meta_path}")
    if "artifacts/physical_bringup/" in str(session_dir).replace("\\", "/"):
        if not manifest_path.exists():
            raise RuntimeError(f"missing physical evidence manifest: {manifest_path}")
        manifest = manifest_path.read_text(encoding="ascii", errors="replace")
        required_manifest = (
            "schema=physical-evidence-v1",
            "target_support_cell=intel-x86_64+uefi+sata-ahci+gop+keyboard",
            "evidence_scope=physical-candidate",
            f"capture_log={log_path.name}",
            "capture_log_sha256=",
            "operator_notes=operator-notes.txt",
            "operator_notes_sha256=",
            "machine=machine.txt",
            "machine_sha256=",
            "pci_nic_present=",
            "nic_vendor_id=",
            "nic_device_id=",
            "nic_class=",
            "nic_driver_family=",
            "nic_bind=",
            "link_state=",
            "mac_address=",
            "network_mode=",
            "runtime_consequence=",
            "blocker=",
        )
        for marker in required_manifest:
            if marker not in manifest:
                raise RuntimeError(f"manifest missing {marker}: {manifest_path}")

    summary = summary_path.read_text(encoding="ascii", errors="replace")
    required = (
        "[boot]",
        "[storage]",
        "[serial]",
        "[display]",
        "[input]",
        "[network]",
        "[platform]",
        "[progress]",
    )
    for marker in required:
        if marker not in summary:
            raise RuntimeError(f"summary missing {marker}: {summary_path}")
    if "[USER] shell ready" not in summary:
        raise RuntimeError(f"summary missing shell-ready evidence: {summary_path}")

    classification = classification_path.read_text(encoding="ascii", errors="replace")
    required_classification = (
        "[identity]",
        "[contracts]",
        "[failure]",
        "[triage]",
        "fwblk=",
        "lsys=ok",
        "progress=",
        "split_layer=",
        "storage_residual_region=",
        "pci_nic_present=",
        "nic_driver_family=",
        "nic_bind=",
        "network_mode=",
    )
    for marker in required_classification:
        if marker not in classification:
            raise RuntimeError(
                f"classification missing {marker}: {classification_path}"
            )
    if "split_layer=none" not in classification:
        raise RuntimeError(
            f"classification expected split_layer=none: {classification_path}"
        )
    if f"firmware={expected_firmware}" not in classification:
        raise RuntimeError(
            f"classification expected firmware={expected_firmware}: {classification_path}"
        )
    if f"progress={expected_progress}" not in classification:
        raise RuntimeError(
            f"classification expected progress={expected_progress}: {classification_path}"
        )

    reference = reference_path.read_text(encoding="ascii", errors="replace")
    if expected_selected_delta_layers is None:
        expected_selected_delta_layers = expected_delta_layers
    if expected_selected_priority_blocker is None:
        expected_selected_priority_blocker = expected_priority_blocker
    if expected_selected_driver_family_delta is None:
        expected_selected_driver_family_delta = expected_driver_family_delta
    required_reference = (
        "status=selected",
        f"selected={expected_reference}",
        "selected_healthy=yes",
        f"actual_firmware={expected_firmware}",
        f"selected_delta_layers={expected_selected_delta_layers}",
        f"selected_priority_blocker={expected_selected_priority_blocker}",
        f"selected_driver_family_delta={expected_selected_driver_family_delta}",
        "candidate=",
    )
    for marker in required_reference:
        if marker not in reference:
            raise RuntimeError(f"reference missing {marker}: {reference_path}")

    delta = delta_path.read_text(encoding="ascii", errors="replace")
    required_delta = (
        "[summary]",
        "status=",
        "delta_layers=",
        "priority_blocker=",
        "driver_family_delta=",
        "[identity]",
        "[contracts]",
        "[failure]",
    )
    for marker in required_delta:
        if marker not in delta:
            raise RuntimeError(f"delta missing {marker}: {delta_path}")
    expected_delta_markers = (
        f"status={expected_delta_status}",
        f"delta_layers={expected_delta_layers}",
        f"priority_blocker={expected_priority_blocker}",
        f"driver_family_delta={expected_driver_family_delta}",
    )
    for marker in expected_delta_markers:
        if marker not in delta:
            raise RuntimeError(f"delta missing expected marker {marker}: {delta_path}")

    verdict = verdict_path.read_text(encoding="ascii", errors="replace")
    required_verdict = (
        "[verdict]",
        f"firmware={expected_firmware}",
        f"progress={expected_progress}",
        "split_layer=none",
        f"priority_blocker={expected_priority_blocker}",
        f"reference={expected_reference}",
        "reference_healthy=yes",
        f"delta_layers={expected_delta_layers}",
        f"driver_family_delta={expected_driver_family_delta}",
        "target_support_cell=intel-x86_64+uefi+sata-ahci+gop+keyboard",
        f"evidence_scope={expected_evidence_scope}",
        f"physical_evidence_status={expected_physical_status}",
        "usb_hid_bind_state=",
        "usb_hid_blocker=",
        "pci_nic_present=",
        "nic_driver_family=",
        "nic_bind=",
        "network_mode=",
        "network_blocker=",
        "support_cell_runtime_gate=",
        "runtime_consequence=",
        f"support_cell_status={expected_support_cell_status}",
        "storage_residual_region=(none)",
        "next_action=",
    )
    for marker in required_verdict:
        if marker not in verdict:
            raise RuntimeError(f"verdict missing {marker}: {verdict_path}")
    if (
        expected_runtime_consequence
        and f"runtime_consequence={expected_runtime_consequence}" not in verdict
    ):
        raise RuntimeError(
            f"verdict expected runtime_consequence={expected_runtime_consequence}: {verdict_path}"
        )
    if expected_physical_blocker and expected_physical_blocker not in verdict:
        raise RuntimeError(
            f"verdict missing physical evidence blocker {expected_physical_blocker}: {verdict_path}"
        )
    if manifest_path.exists() and "evidence-manifest-" in verdict:
        raise RuntimeError(f"verdict reported physical evidence manifest blocker: {verdict_path}")

    meta = meta_path.read_text(encoding="ascii", errors="replace")
    required_meta = (
        "log=",
        "reference=",
        f"evidence_scope={expected_evidence_scope}",
        f"physical_evidence_status={expected_physical_status}",
        "usb_hid_bind_state=",
        "usb_hid_blocker=",
        "pci_nic_present=",
        "nic_driver_family=",
        "nic_bind=",
        "link_state=",
        "network_mode=",
        "network_blocker=",
        "firmware=",
        "split_layer=",
        "priority_blocker=",
        "driver_family_delta=",
        f"support_cell_status={expected_support_cell_status}",
        "support_cell_runtime_gate=",
        "runtime_consequence=",
        "generated_utc=",
    )
    for marker in required_meta:
        if marker not in meta:
            raise RuntimeError(f"metadata missing {marker}: {meta_path}")
    if (
        "split_layer=none" not in meta
        or f"priority_blocker={expected_priority_blocker}" not in meta
        or f"driver_family_delta={expected_driver_family_delta}" not in meta
    ):
        raise RuntimeError(
            f"metadata expected split_layer=none, priority_blocker={expected_priority_blocker}, and driver_family_delta={expected_driver_family_delta}: {meta_path}"
        )
    if f"reference={expected_reference}" not in meta:
        raise RuntimeError(f"metadata expected reference={expected_reference}: {meta_path}")
    if f"firmware={expected_firmware}" not in meta:
        raise RuntimeError(f"metadata expected firmware={expected_firmware}: {meta_path}")


def cleanup_session(session_dir: Path) -> None:
    if session_dir.exists():
        shutil.rmtree(session_dir)


def write_physical_candidate_inputs(session_dir: Path) -> Path:
    notes_path = session_dir / "operator-notes.txt"
    notes_path.write_text(
        "\n".join(
            (
                "machine_model=Codex Synthetic Physical Candidate",
                "firmware_version=UEFI-test-1",
                "sata_mode=AHCI",
                "usb_port=rear-usb2",
                "capture_source=operator-transcript",
                "last_visible_line=[USER] shell ready",
                "shell_ready=yes",
                "gop_result=ready",
                "keyboard_result=ready",
            )
        )
        + "\n",
        encoding="ascii",
    )
    log_path = session_dir / "physical-capture.log"
    log_path.write_text(
        "\n".join(
            (
                "LunaLoader UEFI Stage 1 handoff",
                "LunaLoader UEFI Stage 1 boot-services ok",
                "LunaLoader UEFI Stage 1 stage-alloc ok",
                "LunaLoader UEFI Stage 1 scratch-alloc ok",
                "LunaLoader UEFI Stage 1 loaded-image ok",
                "LunaLoader UEFI Stage 1 stage-load ok",
                "LunaLoader UEFI Stage 1 manifest ok",
                "LunaLoader UEFI Stage 1 block-io ready",
                "LunaLoader UEFI Stage 1 gop ready",
                "LunaLoader UEFI Stage 1 jump stage",
                "[BOOT] dawn online",
                "[BOOT] fwblk handoff ok",
                "[BOOT] lsys super read ok",
                "[BOOT] native pair ok",
                "[BOOT] continuing boot",
                "[DEVICE] disk path driver=ahci family=0000000E chain=ahci>fwblk>ata mode=normal",
                "[DEVICE] disk select basis=ahci-runtime fwblk-src=ready fwblk-tgt=ready separate=missing ahci=ready pci=ready",
                "[DEVICE] disk pci vendor=8086 device=2922 bdf=00:1F.02 class=01/06/01 hdr=80",
                "[DEVICE] disk ahci port=00 abar=0000000081084000",
                "[DEVICE] serial path driver=ich9-uart family=00000013",
                "[DEVICE] serial select basis=ich9-pci pci=ready",
                "[DEVICE] serial pci vendor=8086 device=2918 bdf=00:1F.00 class=06/01/00 hdr=80",
                "[DEVICE] display path driver=boot-fb family=0000000F fb=ready size=00000000003E8000 w=00000500 h=00000320 stride=00000500 fmt=00000001",
                "[DEVICE] display select basis=gop-fb gop=ready pci=ready",
                "[DEVICE] display pci vendor=1234 device=1111 bdf=00:01.00 class=03/00/00 hdr=00",
                "[DEVICE] input path kbd=i8042-kbd ptr=i8042-mouse virtio=missing ps2=present lane=ready",
                "[DEVICE] input select basis=i8042 virtio-dev=missing virtio-ready=missing legacy=ready usb-ctrl=ready usb-hid=not-bound usb-hid-blocker=controller-driver-missing",
                "[DEVICE] input ctrl legacy=i8042",
                "[DEVICE] input usb candidate ctrl=xhci hid=not-bound blocker=controller-driver-missing owner=DEVICE consequence=degraded-continue",
                "[DEVICE] input usb pci vendor=8086 device=1E31 bdf=00:14.00 class=0C/03/30 hdr=00",
                "[DEVICE] net path driver=e1000e family=0000000D lane=ready mmio=0000000081060000",
                "[DEVICE] net select basis=e1000e-ready pci=ready live=ready",
                "[DEVICE] net pci vendor=8086 device=10D3 bdf=00:02.00 class=02/00/00 hdr=00",
                "[DEVICE] net driver family=e1000e bind=ready blocker=none",
                "[DEVICE] net link state=up status=00000002 mode=external raw=ethertype-88B5",
                "[DEVICE] net mac=52:54:00:12:34:56 mmio=0000000081060000",
                "[DEVICE] platform pci vendor=8086 device=29C0 bdf=00:00.00 class=06/00/00 hdr=00",
                "[DEVICE] lane ready",
                "[USER] shell ready",
                "first-setup required: no hostname or user configured",
                "[DESKTOP] status launcher=closed control=closed theme=0 selected=Settings update=idle pair=unavailable policy=deny",
            )
        )
        + "\n",
        encoding="ascii",
    )
    return log_path


def main() -> int:
    pwsh = find_pwsh()
    physical_name = "codex-physical-smoke"
    physical_candidate_name = "codex-physical-candidate-smoke"
    vmware_name = "codex-vmware-smoke"

    physical_dir: Path | None = None
    physical_candidate_dir: Path | None = None
    vmware_dir: Path | None = None
    try:
        subprocess.run(
            [sys.executable, str(ROOT / "run_firsthop_classifier_selftest.py")],
            cwd=REPO_ROOT,
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        physical_proc = run_ps(
            pwsh,
            ROOT / "start_physical_bringup_session.ps1",
            "-Machine",
            physical_name,
            "-VersionStem",
            "dev",
        )
        physical_dir = extract_session_path(physical_proc.stdout)

        physical_candidate_proc = run_ps(
            pwsh,
            ROOT / "start_physical_bringup_session.ps1",
            "-Machine",
            physical_candidate_name,
            "-VersionStem",
            "dev",
        )
        physical_candidate_dir = extract_session_path(physical_candidate_proc.stdout)

        vmware_proc = run_ps(
            pwsh,
            ROOT / "start_vmware_bringup_session.ps1",
            "-Machine",
            vmware_name,
            "-VersionStem",
            "dev",
        )
        vmware_dir = extract_session_path(vmware_proc.stdout)

        assert_summary(
            physical_dir,
            ROOT / "qemu_bootcheck.log",
            "qemu_bootcheck.log",
            "legacy",
            "shell-ready-first-setup",
            "virtualized-prephysical",
            "missing",
            "not-established-prephysical-only",
            "virtualized-log-name",
        )
        physical_candidate_log = write_physical_candidate_inputs(physical_candidate_dir)
        assert_summary(
            physical_candidate_dir,
            physical_candidate_log,
            "qemu_uefi_shellcheck.log",
            "uefi",
            "desktop-first-setup",
            "physical-candidate",
            "present",
            "not-established-physical-candidate-needs-review",
            expected_runtime_consequence="continue",
            expected_delta_status="diverged",
            expected_delta_layers="driver-family",
            expected_priority_blocker="driver-family",
            expected_driver_family_delta="display:std-vga-fb/std-vga-fb->boot-fb/gop-fb",
        )
        assert_summary(
            vmware_dir,
            ROOT / "qemu_uefi_shellcheck.log",
            "qemu_uefi_shellcheck.log",
            "uefi",
            "desktop-first-setup",
            "virtualized-prephysical",
            "not-applicable",
            "not-established-prephysical-only",
        )

        sys.stdout.write(
            "bringup session smoke ok: "
            f"{physical_dir.name} {physical_candidate_dir.name} {vmware_dir.name}\n"
        )
        return 0
    finally:
        if physical_dir and ARTIFACTS_ROOT in physical_dir.parents:
            cleanup_session(physical_dir)
        if physical_candidate_dir and ARTIFACTS_ROOT in physical_candidate_dir.parents:
            cleanup_session(physical_candidate_dir)
        if vmware_dir and ARTIFACTS_ROOT in vmware_dir.parents:
            cleanup_session(vmware_dir)


if __name__ == "__main__":
    raise SystemExit(main())
