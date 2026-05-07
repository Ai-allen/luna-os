from __future__ import annotations

import shutil
import uuid
from pathlib import Path

from classify_firsthop import classify
from compare_firsthop import compare
from render_firsthop_verdict import render_verdict
from select_firsthop_baseline import select_baseline


ROOT = Path(__file__).resolve().parent
TMP_ROOT = ROOT / "_selftest_tmp"


def write_case(tmp: Path, name: str, text: str) -> Path:
    path = tmp / name
    path.write_text(text, encoding="utf-8")
    return path


def require(label: str, actual: str, expected: str) -> None:
    if actual != expected:
        raise RuntimeError(f"{label}: expected {expected!r}, got {actual!r}")


def main() -> int:
    tmp = TMP_ROOT / f"firsthop-selftest-{uuid.uuid4().hex}"
    tmp.mkdir(parents=True, exist_ok=True)
    try:

        handoff_path = write_case(
            tmp,
            "handoff.log",
            "\n".join(
                (
                    "[BOOT] dawn online",
                    "[FWBLK] handoff missing",
                )
            )
            + "\n",
        )
        handoff = classify(handoff_path)
        require("handoff fwblk", handoff["fwblk"], "missing-blocking")
        require("handoff split", handoff["split_layer"], "handoff")

        storage_path = write_case(
            tmp,
            "storage.log",
            "\n".join(
                (
                    "LunaLoader UEFI Stage 1 handoff",
                    "[BOOT] dawn online",
                    "[BOOT] fwblk handoff ok",
                    "[BOOT] lsys read fail",
                    "[DEVICE] disk path driver=ahci family=0000000E chain=ahci>fwblk>ata mode=normal",
                )
            )
            + "\n",
        )
        storage = classify(storage_path)
        require("storage fwblk", storage["fwblk"], "ready")
        require("storage lsys", storage["lsys"], "fail")
        require("storage split", storage["split_layer"], "storage")
        require("storage lane", storage["storage_lane"], "degraded")

        residual_path = write_case(
            tmp,
            "residual.log",
            "\n".join(
                (
                    "[DEVICE] disk layout lsys=00004800 ldat=00008800 target=0000000000000000",
                    "[DISK] write fail lba=00008810",
                )
            )
            + "\n",
        )
        residual = classify(residual_path)
        require("residual region", residual["storage_residual_region"], "ldat+0x0010")

        input_path = write_case(
            tmp,
            "input.log",
            "\n".join(
                (
                    "[BOOT] dawn online",
                    "[FWBLK] handoff missing",
                    "[BOOT] lsys super read ok",
                    "[BOOT] native pair ok",
                    "[DEVICE] input path kbd=i8042-kbd ptr=i8042-mouse virtio=missing ps2=present lane=ready",
                    "[DEVICE] input select basis=i8042 virtio-dev=missing virtio-ready=missing legacy=ready usb-ctrl=ready usb-hid=not-bound usb-hid-blocker=controller-driver-missing",
                    "[DEVICE] input ctrl legacy=i8042",
                    "[DEVICE] input usb candidate ctrl=xhci hid=not-bound blocker=controller-driver-missing owner=DEVICE consequence=degraded-continue",
                    "[DEVICE] input usb pci vendor=8086 device=1E31 bdf=00:14.00 class=0C/03/30 hdr=00",
                    "[VIRTKBD] pci missing",
                    "[USER] shell ready",
                )
            )
            + "\n",
        )
        input_ready = classify(input_path)
        require("input fwblk", input_ready["fwblk"], "legacy-missing-ok")
        require("input split", input_ready["split_layer"], "none")
        require("input lane", input_ready["input_lane"], "ready")
        require("input usb hid state", input_ready["usb_hid_bind_state"], "not-bound")
        require("input usb blocker", input_ready["usb_hid_blocker"], "controller-driver-missing")

        usb_hid_blocked_path = write_case(
            tmp,
            "usb-hid-blocked.log",
            "\n".join(
                (
                    "LunaLoader UEFI Stage 1 handoff",
                    "[BOOT] dawn online",
                    "[BOOT] fwblk handoff ok",
                    "[BOOT] lsys super read ok",
                    "[BOOT] native pair ok",
                    "[DEVICE] input path kbd=i8042-kbd ptr=i8042-mouse virtio=missing ps2=missing lane=offline",
                    "[DEVICE] input select basis=legacy-kbd virtio-dev=missing virtio-ready=missing legacy=missing usb-ctrl=ready usb-hid=not-bound usb-hid-blocker=controller-driver-missing",
                    "[DEVICE] input ctrl legacy=missing",
                    "[DEVICE] input usb candidate ctrl=xhci hid=not-bound blocker=controller-driver-missing owner=DEVICE consequence=degraded-continue",
                    "[DEVICE] input usb pci vendor=1B36 device=000D bdf=00:03.00 class=0C/03/30 hdr=00",
                    "[USER] shell ready",
                )
            )
            + "\n",
        )
        usb_hid_blocked = classify(usb_hid_blocked_path)
        require("usb hid blocked split", usb_hid_blocked["split_layer"], "input")
        require("usb hid blocked lane", usb_hid_blocked["input_lane"], "offline")
        require("usb hid blocked state", usb_hid_blocked["usb_hid_bind_state"], "not-bound")
        require("usb hid blocked blocker", usb_hid_blocked["usb_hid_blocker"], "controller-driver-missing")
        require("usb hid blocked runtime", usb_hid_blocked["runtime_consequence"], "degraded-input-recovery")

        uefi_boot_only_path = write_case(
            tmp,
            "uefi-boot-only.log",
            "\n".join(
                (
                    "[BOOT] dawn online",
                    "[BOOT] fwblk magic=49464555 version=00000002 status=0000000000000009",
                    "[BOOT] fwblk handoff ok",
                )
            )
            + "\n",
        )
        uefi_boot_only = classify(uefi_boot_only_path)
        require("uefi boot-only firmware", uefi_boot_only["firmware"], "uefi")

        selection_path = write_case(
            tmp,
            "selection.log",
            "\n".join(
                (
                    "[DEVICE] serial select basis=piix-pci pci=ready",
                    "[DEVICE] disk select basis=fwblk-target fwblk-src=ready fwblk-tgt=ready separate=ready ahci=missing pci=ready",
                    "[DEVICE] display select basis=gop-fb gop=ready pci=ready",
                    "[DEVICE] input select basis=i8042 virtio-dev=missing virtio-ready=missing legacy=ready",
                    "[DEVICE] net select basis=e1000-ready pci=ready live=ready",
                )
            )
            + "\n",
        )
        selection = classify(selection_path)
        require(
            "selection profile",
            selection["selection_profile"],
            "storage=fwblk-target,serial=piix-pci,display=gop-fb,input=i8042,net=e1000-ready",
        )

        governance_path = write_case(
            tmp,
            "governance.log",
            "\n".join(
                (
                    "LunaLoader UEFI Stage 1 handoff",
                    "[BOOT] dawn online",
                    "[BOOT] fwblk handoff ok",
                    "[BOOT] lsys super read ok",
                    "[BOOT] native pair ok",
                    "allow_device_call denied gate=device.write status=deny",
                )
            )
            + "\n",
        )
        governance = classify(governance_path)
        require("governance split", governance["split_layer"], "driver-governance")

        audit_only_governance_path = write_case(
            tmp,
            "audit-only-governance.log",
            "\n".join(
                (
                    "LunaLoader UEFI Stage 1 handoff",
                    "[BOOT] dawn online",
                    "[BOOT] fwblk handoff ok",
                    "[BOOT] lsys super read ok",
                    "[BOOT] native pair ok",
                    "audit driver.bind denied=SECURITY",
                    "[USER] shell ready",
                )
            )
            + "\n",
        )
        audit_only_governance = classify(audit_only_governance_path)
        require("audit-only governance split", audit_only_governance["split_layer"], "none")
        require("audit-only governance evidence", audit_only_governance["driver_governance"], "(none)")

        reference_path = write_case(
            tmp,
            "reference.log",
            "\n".join(
                (
                    "LunaLoader UEFI Stage 1 handoff",
                    "[BOOT] dawn online",
                    "[BOOT] fwblk handoff ok",
                    "[BOOT] lsys super read ok",
                    "[BOOT] native pair ok",
                    "[DEVICE] disk path driver=ahci family=0000000E chain=ahci>fwblk>ata mode=normal",
                    "[DEVICE] input path kbd=virtio-kbd ptr=i8042-mouse virtio=ready ps2=present lane=ready",
                    "[USER] shell ready",
                    "first-setup required: no hostname or user configured",
                )
            )
            + "\n",
        )
        compare_storage_path = write_case(
            tmp,
            "compare-storage.log",
            "\n".join(
                (
                    "LunaLoader UEFI Stage 1 handoff",
                    "[BOOT] dawn online",
                    "[BOOT] fwblk handoff ok",
                    "[BOOT] lsys read fail",
                    "[DEVICE] disk path driver=ahci family=0000000E chain=ahci>fwblk>ata mode=normal",
                )
            )
            + "\n",
        )
        storage_delta = compare(reference_path, compare_storage_path)
        if "delta_layers=" not in storage_delta or "storage" not in storage_delta:
            raise RuntimeError(f"compare storage delta mismatch: {storage_delta}")
        if "priority_blocker=storage" not in storage_delta:
            raise RuntimeError(f"compare storage blocker mismatch: {storage_delta}")

        compare_governance_path = write_case(
            tmp,
            "compare-governance.log",
            "\n".join(
                (
                    "LunaLoader UEFI Stage 1 handoff",
                    "[BOOT] dawn online",
                    "[BOOT] fwblk handoff ok",
                    "[BOOT] lsys super read ok",
                    "[BOOT] native pair ok",
                    "[DEVICE] disk path driver=ahci family=0000000E chain=ahci>fwblk>ata mode=normal",
                    "[DEVICE] input path kbd=virtio-kbd ptr=i8042-mouse virtio=ready ps2=present lane=ready",
                    "allow_device_call denied gate=device.write status=deny",
                )
            )
            + "\n",
        )
        governance_delta = compare(reference_path, compare_governance_path)
        if "priority_blocker=driver-governance" not in governance_delta:
            raise RuntimeError(f"compare governance blocker mismatch: {governance_delta}")

        storage_verdict = render_verdict(compare_storage_path)
        if "priority_blocker=storage" not in storage_verdict:
            raise RuntimeError(f"verdict storage blocker mismatch: {storage_verdict}")
        if "next_action=check lsys/native pair read path" not in storage_verdict:
            raise RuntimeError(f"verdict storage action mismatch: {storage_verdict}")

        governance_verdict = render_verdict(compare_governance_path)
        if "priority_blocker=driver-governance" not in governance_verdict:
            raise RuntimeError(f"verdict governance blocker mismatch: {governance_verdict}")

        live_legacy = select_baseline(ROOT / "qemu_bootcheck.log")
        require("live legacy baseline", str(live_legacy["selected"]), "qemu_bootcheck.log")

        live_uefi = select_baseline(ROOT / "qemu_uefi_shellcheck.log")
        require("live uefi baseline", str(live_uefi["selected"]), "qemu_uefi_shellcheck.log")
        require("live uefi healthy", str(live_uefi["selected_healthy"]), "yes")

        live_vmware_path = ROOT / "vmware_desktopcheck.serial.log"
        expected_vmware_blocker = "driver-family"
        if not live_vmware_path.exists():
            live_vmware_path = ROOT / "vmware_desktopcheck.vmware.log"
        if not live_vmware_path.exists():
            raise FileNotFoundError(live_vmware_path)
        live_vmware = select_baseline(live_vmware_path)
        require("live vmware selected", str(live_vmware["selected"]), "qemu_uefi_shellcheck.log")
        require("live vmware healthy", str(live_vmware["selected_healthy"]), "yes")
        require("live vmware blocker", str(live_vmware["selected_priority_blocker"]), expected_vmware_blocker)

        print("firsthop classifier selftest ok")
        return 0
    finally:
        if tmp.exists():
            shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
