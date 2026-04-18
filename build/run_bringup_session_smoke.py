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
    required_reference = (
        "status=selected",
        f"selected={expected_reference}",
        "selected_healthy=yes",
        f"actual_firmware={expected_firmware}",
        "selected_driver_family_delta=none",
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
    if (
        "status=aligned" not in delta
        or "delta_layers=none" not in delta
        or "priority_blocker=none" not in delta
        or "driver_family_delta=none" not in delta
    ):
        raise RuntimeError(f"delta expected aligned baseline: {delta_path}")

    verdict = verdict_path.read_text(encoding="ascii", errors="replace")
    required_verdict = (
        "[verdict]",
        f"firmware={expected_firmware}",
        f"progress={expected_progress}",
        "split_layer=none",
        "priority_blocker=none",
        f"reference={expected_reference}",
        "reference_healthy=yes",
        "delta_layers=none",
        "driver_family_delta=none",
        "storage_residual_region=(none)",
        "next_action=",
    )
    for marker in required_verdict:
        if marker not in verdict:
            raise RuntimeError(f"verdict missing {marker}: {verdict_path}")

    meta = meta_path.read_text(encoding="ascii", errors="replace")
    required_meta = (
        "log=",
        "reference=",
        "firmware=",
        "split_layer=",
        "priority_blocker=",
        "driver_family_delta=",
        "generated_utc=",
    )
    for marker in required_meta:
        if marker not in meta:
            raise RuntimeError(f"metadata missing {marker}: {meta_path}")
    if (
        "split_layer=none" not in meta
        or "priority_blocker=none" not in meta
        or "driver_family_delta=none" not in meta
    ):
        raise RuntimeError(
            f"metadata expected split_layer=none, priority_blocker=none, and driver_family_delta=none: {meta_path}"
        )
    if f"reference={expected_reference}" not in meta:
        raise RuntimeError(f"metadata expected reference={expected_reference}: {meta_path}")
    if f"firmware={expected_firmware}" not in meta:
        raise RuntimeError(f"metadata expected firmware={expected_firmware}: {meta_path}")


def cleanup_session(session_dir: Path) -> None:
    if session_dir.exists():
        shutil.rmtree(session_dir)


def main() -> int:
    pwsh = find_pwsh()
    physical_name = "codex-physical-smoke"
    vmware_name = "codex-vmware-smoke"

    physical_dir: Path | None = None
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
        )
        assert_summary(
            vmware_dir,
            ROOT / "qemu_uefi_shellcheck.log",
            "qemu_uefi_shellcheck.log",
            "uefi",
            "desktop-first-setup",
        )

        sys.stdout.write(
            "bringup session smoke ok: "
            f"{physical_dir.name} {vmware_dir.name}\n"
        )
        return 0
    finally:
        if physical_dir and ARTIFACTS_ROOT in physical_dir.parents:
            cleanup_session(physical_dir)
        if vmware_dir and ARTIFACTS_ROOT in vmware_dir.parents:
            cleanup_session(vmware_dir)


if __name__ == "__main__":
    raise SystemExit(main())
