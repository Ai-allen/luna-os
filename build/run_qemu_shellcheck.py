from __future__ import annotations

import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent
IMAGE = ROOT / "lunaos.img"
SHELLCHECK_IMAGE = ROOT / "lunaos_shellcheck.img"
LOG_PATH = ROOT / "qemu_shellcheck.log"
ERR_PATH = ROOT / "qemu_shellcheck.err.log"
BOOT_SECTOR_BYTES = 512
SESSION_MAGIC = 0x54504353
DEV_BUNDLE_STAGE_MAGIC = 0x4244564C


def find_qemu() -> str:
    found = shutil.which("qemu-system-x86_64")
    if found:
        return found
    for candidate in (
        r"C:\Program Files\qemu\qemu-system-x86_64.exe",
        r"C:\Program Files\QEMU\qemu-system-x86_64.exe",
    ):
        if Path(candidate).exists():
            return candidate
    raise FileNotFoundError("qemu-system-x86_64 not found. Install QEMU or add it to PATH.")


def parse_session_layout() -> tuple[int, int]:
    image_base = None
    for line in (ROOT / "luna.ld").read_text(encoding="utf-8").splitlines():
        parts = line.strip().split()
        if len(parts) == 2 and parts[0] == "IMAGE_BASE":
            image_base = int(parts[1], 16)
        if len(parts) == 4 and parts[0] == "SCRIPT" and parts[1] == "SESSION":
            if image_base is None:
                raise RuntimeError("missing IMAGE_BASE entry")
            return image_base, int(parts[2], 16), int(parts[3], 16)
    raise RuntimeError("missing SCRIPT SESSION layout entry")


def patch_session_script(commands: bytes, staged_bundle: bytes = b"") -> None:
    image_base, script_base, script_size = parse_session_layout()
    payload_capacity = script_size - 8
    extra = b""
    if staged_bundle:
        extra = struct.pack("<II", DEV_BUNDLE_STAGE_MAGIC, len(staged_bundle)) + staged_bundle
    if len(commands) + len(extra) > payload_capacity:
        raise RuntimeError("session script exceeds configured capacity")

    loader_path = ROOT / "obj" / "lunaloader_stage1.bin"
    if not loader_path.exists():
        loader_path = ROOT / "build" / "obj" / "lunaloader_stage1.bin"
    if not loader_path.exists():
        raise RuntimeError("missing lunaloader_stage1.bin")
    loader_sectors = (loader_path.stat().st_size + BOOT_SECTOR_BYTES - 1) // BOOT_SECTOR_BYTES
    stage_offset = BOOT_SECTOR_BYTES + loader_sectors * BOOT_SECTOR_BYTES

    blob = bytearray(IMAGE.read_bytes())
    offset = stage_offset + (script_base - image_base)
    header = struct.pack("<II", SESSION_MAGIC, len(commands))
    payload = header + commands + extra + (b"\x00" * (script_size - len(header) - len(commands) - len(extra)))
    blob[offset:offset + script_size] = payload
    SHELLCHECK_IMAGE.write_bytes(blob)


def pack_devloop_bundle() -> bytes:
    repo_root = ROOT.parent
    output = ROOT / "devloop_sample.luna"
    subprocess.run(
        [
            sys.executable,
            str(repo_root / "tools" / "luna_pack.py"),
            str(repo_root / "tools" / "luna_manifest.minimal.json"),
            str(repo_root / "build" / "obj" / "console_app.bin"),
            str(output),
        ],
        cwd=repo_root,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return output.read_bytes()


def main() -> int:
    qemu = find_qemu()
    commands = (
        b"setup.status\r\n"
        b"setup.init luna dev secret\r\n"
        b"whoami\r\n"
        b"hostname\r\n"
        b"home.status\r\n"
        b"cap-count\r\n"
        b"cap-list\r\n"
        b"lasql.catalog\r\n"
        b"lasql.files\r\n"
        b"lasql.logs\r\n"
        b"seal-list\r\n"
        b"space-map\r\n"
        b"store-info\r\n"
        b"store-check\r\n"
        b"list-apps\r\n"
        b"run Files\r\n"
        b"run Guard\r\n"
        b"package.install sample\r\n"
        b"list-apps\r\n"
        b"run sample\r\n"
        b"package.remove sample\r\n"
        b"list-apps\r\n"
        b"run sample\r\n"
        b"space-log\r\n"
        b"lane-census\r\n"
        b"pci-scan\r\n"
        b"net.loop\r\n"
        b"net.info\r\n"
        b"net.external\r\n"
        b"revoke-cap program.load\r\n"
        b"cap-count\r\n"
        b"run Settings\r\n"
        b"settings.status\r\n"
        b"bogus\r\n"
        b"revoke-cap device.list\r\n"
        b"cap-count\r\n"
        b"list-devices\r\n"
    )
    patch_session_script(commands, pack_devloop_bundle())
    if LOG_PATH.exists():
        LOG_PATH.unlink()
    if ERR_PATH.exists():
        ERR_PATH.unlink()

    with LOG_PATH.open("w", encoding="utf-8", errors="replace") as stdout_file, ERR_PATH.open("w", encoding="utf-8", errors="replace") as stderr_file:
        proc = subprocess.Popen(
            [
                qemu,
                "-drive",
                f"format=raw,file={SHELLCHECK_IMAGE}",
                "-serial",
                "stdio",
                "-display",
                "none",
                "-no-reboot",
                "-no-shutdown",
            ],
            stdout=stdout_file,
            stderr=stderr_file,
            text=True,
            encoding="utf-8",
            errors="replace",
        )

        try:
            deadline = time.time() + 90.0
            while time.time() < deadline:
                if LOG_PATH.exists():
                    text = LOG_PATH.read_text(encoding="utf-8", errors="replace")
                    if "list-devices failed" in text and "USER booting USRBOOTI" in text:
                        break
                if proc.poll() is not None:
                    break
                time.sleep(0.25)
            if proc.poll() is None:
                proc.kill()
            proc.wait(timeout=5)
        finally:
            if proc.poll() is None:
                proc.kill()

    stdout = LOG_PATH.read_text(encoding="utf-8", errors="replace")
    stderr = ERR_PATH.read_text(encoding="utf-8", errors="replace")

    required = [
        "[USER] shell ready",
        "first-setup required: no hostname or user configured",
        "[DEVICE] fwblk source=missing target=missing",
        "[DEVICE] disk path driver=piix-ide family=0000000C chain=ahci>fwblk>ata mode=normal",
        "[DEVICE] disk select basis=piix-ide fwblk-src=missing fwblk-tgt=missing separate=missing ahci=missing pci=ready",
        "[DEVICE] disk pci vendor=8086 device=7010 bdf=00:01.01 class=01/01/80 hdr=00",
        "[DEVICE] serial path driver=piix-uart family=00000012",
        "[DEVICE] serial select basis=piix-pci pci=ready",
        "[DEVICE] serial pci vendor=8086 device=7000 bdf=00:01.00 class=06/01/00 hdr=80",
        "[DEVICE] display path driver=std-vga-text family=0000000F",
        "[DEVICE] display select basis=std-vga-text gop=missing pci=ready",
        "[DEVICE] display pci vendor=1234 device=1111 bdf=00:02.00 class=03/00/00 hdr=00",
        "[DEVICE] input path kbd=i8042-kbd ptr=i8042-mouse virtio=missing ps2=present lane=ready",
        "[DEVICE] input select basis=i8042 virtio-dev=missing virtio-ready=missing legacy=ready",
        "[DEVICE] input ctrl legacy=i8042",
        "[DEVICE] net path driver=e1000 family=0000000B lane=ready",
        "[DEVICE] net select basis=e1000-ready pci=ready live=ready",
        "[DEVICE] net pci vendor=8086 device=100E bdf=00:03.00 class=02/00/00 hdr=00",
        "[DEVICE] platform pci vendor=8086 device=1237 bdf=00:00.00 class=06/00/00 hdr=00",
        "setup.status",
        "setup state=required login=locked user=guest host=luna",
        "setup.init luna dev secret",
        "setup.init ok: host and first user created",
        "login ok: session active",
        "whoami",
        "dev",
        "hostname",
        "luna",
        "home.status",
        "home owner=dev@luna",
        "home.documents=0x",
        "cap-count",
        "caps: 023",
        "cap-list",
        "user.shell uses=",
        "system.query uses=",
        "program.load uses=",
        "device.list uses=",
        "graphics.draw uses=",
        "lasql.catalog",
        "audit lasql.query govern allow",
        "audit lasql.query package",
        "lasql.catalog ok",
        "lasql.files",
        "audit lasql.query files",
        "lasql.files ok",
        "lasql.logs",
        "lasql.logs ok",
        "audit lasql.query files type=",
        "audit lasql.query package type=",
        "audit package.query source=lasql kind=list",
        "audit package.query source=lasql kind=resolve",
        "seal-list",
        "seals: 0",
        "space-map",
        "BOOT state=active mem=0 time=0 event=BOOTLIVE",
        "SYSTEM state=active mem=0 time=2 event=SYSREADY",
        "DATA state=active mem=8 time=0 event=LAFSDISK",
        "USER state=booting mem=0 time=0 event=USRBOOTI",
        "store-info",
        "lafs.version: 3",
        "lafs.objects: 34",
        "lafs.state: clean",
        "lafs.mounts: 1",
        "lafs.formats: 1",
        "lafs.nonce: 97",
        "lafs.last-repair: none",
        "lafs.last-scrubbed: 0",
        "store-check",
        "lafs.health: ok",
        "lafs.invalid: 0",
        "list-apps",
        "Settings",
        "Notes",
        "Files",
        "Guard",
        "Console",
        "run Files",
        "files surface ready",
        "files.user",
        "files.visible=",
        "files.current=",
        "run Guard",
        "security center",
        "observe entries:",
        "observe level:",
        "package.install sample",
        "audit package.install approved=SECURITY",
        "audit package.install persisted=DATA authority=PACKAGE",
        "package.install ok",
        "package.remove sample",
        "audit package.remove approved=SECURITY",
        "audit package.remove persisted=DATA authority=PACKAGE",
        "package.remove ok",
        "space-log",
        "USER booting USRBOOTI",
        "lane-census",
        "serial0 kind=serial class=stream driver=piix-uart flags=present|ready|boot id=1",
        "disk0 kind=disk class=block driver=piix-ide flags=present|ready|boot|fallback|polling id=2",
        "display0 kind=display class=scanout driver=std-vga-text flags=present|ready|boot id=3",
        "clock0 kind=clock class=clock driver=pit-tsc flags=present|ready|boot|polling id=4",
        "input0 kind=input class=input driver=i8042-kbd flags=present|ready|boot|polling id=5",
        "net0 kind=network class=link driver=e1000 flags=present|ready|polling id=6",
        "pointer0 kind=input class=input driver=i8042-mouse flags=present|ready|boot|polling id=7",
        "pci-scan",
        "00:00.0 host-bridge vendor=",
        "00:02.0 vga vendor=",
        "00:03.0 ethernet vendor=",
        "net.loop",
        "net.loop state=ready scope=local-only",
        "net.loop bytes=8 data=luna-net",
        "network state=ready loopback=available external=outbound-only user=dev host=luna",
        "network.entry inspect=net.info verify=net.external diagnostics=net.loop",
        "net.info driver=11 flags=19 vendor=8086 device=100E",
        "net.info mac=52:54:00:12:34:56 mmio=0x0000000010000000",
        "net.info tx_packets=1 rx_packets=0",
        "net.external",
        "net.external state=ready scope=outbound-only",
        "net.external bytes=8 data=luna-ext",
        "revoke-cap program.load",
        "revoked: program.load (1)",
        "caps: 022",
        "run Settings",
        "launch request: Settings",
        "settings surface ready",
        "settings.status",
        "[DESKTOP] status launcher=closed control=closed theme=0 selected=Settings update=idle pair=unavailable policy=deny",
        "settings.user account=dev host=luna session=active",
        "settings.home root=0x",
        "settings.update state=idle current=0 target=0 user=dev",
        "settings.pairing state=unavailable peer=0 user=dev",
        "settings.policy state=deny user=dev",
        "bogus",
        "unknown command; run help",
        "revoke-cap device.list",
        "revoked: device.list (1)",
        "caps: 021",
        "list-devices",
        "list-devices failed",
    ]
    for needle in required:
        if needle not in stdout:
            raise RuntimeError(f"shellcheck missing expected output: {needle}")

    install_list_start = stdout.index("package.install sample")
    install_list_marker = stdout.index("list-apps", install_list_start)
    run_sample_start = stdout.index("run sample", install_list_marker)
    install_block = stdout[install_list_start:run_sample_start]
    for needle in (
        "package.install ok",
        "package.install app=sample.luna status=installed",
        "package.install package=sample.luna",
        "package.install result=visible in Apps and ready to open",
    ):
        if needle not in install_block:
            raise RuntimeError(f"shellcheck sample install missing expected output: {needle}")
    install_list_block = stdout[install_list_marker:run_sample_start].splitlines()
    if "sample.luna" not in install_list_block:
        raise RuntimeError("shellcheck missing installed sample in list-apps output")

    run_sample_remove = stdout.index("package.remove sample", run_sample_start)
    run_sample_block = stdout[run_sample_start:run_sample_remove]
    if "launch request: sample.luna" not in run_sample_block:
        raise RuntimeError("shellcheck missing sample launch request")
    if "apps 1-5 or F/N/G/C/H" not in run_sample_block:
        raise RuntimeError("shellcheck sample run missing visible app output")
    if "launch failed" in run_sample_block:
        raise RuntimeError("shellcheck sample run still failed after install")

    remove_list_marker = stdout.index("list-apps", run_sample_remove)
    remove_run_sample = stdout.index("run sample", remove_list_marker)
    remove_block = stdout[run_sample_remove:remove_list_marker]
    for needle in (
        "package.remove ok",
        "package.remove app=sample status=removed",
        "package.remove package=sample",
        "package.remove result=removed from Apps and launch blocked",
    ):
        if needle not in remove_block:
            raise RuntimeError(f"shellcheck sample remove missing expected output: {needle}")
    remove_list_block = stdout[remove_list_marker:remove_run_sample].splitlines()
    if "sample.luna" in remove_list_block:
        raise RuntimeError("shellcheck sample still present after package.remove")

    remove_run_block = stdout[remove_run_sample:]
    if "launch request: sample" not in remove_run_block or "launch failed" not in remove_run_block:
        raise RuntimeError("shellcheck sample remove path did not end in launch failed")
    if "[PACKAGE]" in stdout or ("[PROGRAM]" in stdout and "audit program." not in stdout):
        raise RuntimeError("shellcheck still exposed internal package/program traces on default surface")

    sys.stdout.write(stdout)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
