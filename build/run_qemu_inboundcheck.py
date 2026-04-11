from __future__ import annotations

import shutil
import socket
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent
IMAGE = ROOT / "lunaos.img"
INBOUND_IMAGE = ROOT / "lunaos_inboundcheck.img"
LOG_PATH = ROOT / "qemu_inboundcheck.log"
ERR_PATH = ROOT / "qemu_inboundcheck.err.log"
BOOT_SECTOR_BYTES = 512
SESSION_MAGIC = 0x54504353
QEMU_SEND_PORT = 41001
QEMU_RECV_PORT = 41002


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


def patch_session_script(commands: bytes) -> None:
    image_base, script_base, script_size = parse_session_layout()
    if len(commands) > script_size - 8:
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
    payload = header + commands + (b"\x00" * (script_size - len(header) - len(commands)))
    blob[offset:offset + script_size] = payload
    INBOUND_IMAGE.write_bytes(blob)


def build_frame() -> bytes:
    payload = b"luna-inbound-ready............................"
    if len(payload) != 46:
        raise RuntimeError("unexpected payload size")
    return (
        bytes.fromhex("52 54 00 12 34 56")
        + bytes.fromhex("02 00 00 00 00 01")
        + bytes.fromhex("88 B6")
        + payload
    )


def main() -> int:
    qemu = find_qemu()
    commands = (
        b"setup.init luna dev secret\r\n"
        b"net.info\r\n"
        b"net.external\r\n"
        b"net.inbound\r\n"
        b"net.inbound\r\n"
        b"net.info\r\n"
    )
    patch_session_script(commands)
    if LOG_PATH.exists():
        LOG_PATH.unlink()
    if ERR_PATH.exists():
        ERR_PATH.unlink()

    host_rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    host_rx.bind(("127.0.0.1", QEMU_SEND_PORT))
    host_rx.setblocking(False)
    host_tx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    with LOG_PATH.open("w", encoding="utf-8", errors="replace") as stdout_file, ERR_PATH.open("w", encoding="utf-8", errors="replace") as stderr_file:
        stdout_chunks: list[str] = []
        stderr_chunks: list[str] = []
        stdout_lock = threading.Lock()
        stderr_lock = threading.Lock()
        stop_sending = threading.Event()

        proc = subprocess.Popen(
            [
                qemu,
                "-drive",
                f"format=raw,file={INBOUND_IMAGE}",
                "-serial",
                "stdio",
                "-display",
                "none",
                "-no-reboot",
                "-no-shutdown",
                "-net",
                "none",
                "-netdev",
                f"socket,id=net0,udp=127.0.0.1:{QEMU_SEND_PORT},localaddr=127.0.0.1:{QEMU_RECV_PORT}",
                "-device",
                "e1000,netdev=net0,mac=52:54:00:12:34:56",
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )

        def pump(stream: subprocess.PIPE, sink_file, sink_chunks: list[str], sink_lock: threading.Lock) -> None:
            try:
                while True:
                    line = stream.readline()
                    if line == "":
                        break
                    sink_file.write(line)
                    sink_file.flush()
                    with sink_lock:
                        sink_chunks.append(line)
            finally:
                stream.close()

        stdout_thread = threading.Thread(
            target=pump,
            args=(proc.stdout, stdout_file, stdout_chunks, stdout_lock),
            daemon=True,
        )
        stderr_thread = threading.Thread(
            target=pump,
            args=(proc.stderr, stderr_file, stderr_chunks, stderr_lock),
            daemon=True,
        )
        stdout_thread.start()
        stderr_thread.start()

        try:
            deadline = time.time() + 30.0
            inbound_ready = False
            inbound_seen = False
            while time.time() < deadline:
                with stdout_lock:
                    text = "".join(stdout_chunks)
                if (
                    not inbound_ready
                    and "network.inbound state=ready entry=net.inbound filter=dst-mac|broadcast" in text
                ):
                    inbound_ready = True
                if (
                    not inbound_seen
                    and "net.inbound state=ready scope=external-raw" in text
                    and "event=seen rx_packets=" in text
                    and "net.info tx_packets=1 rx_packets=1" in text
                ):
                    inbound_seen = True
                if inbound_ready and not inbound_seen:
                    try:
                        frame = build_frame()
                        for _ in range(8):
                            host_tx.sendto(frame, ("127.0.0.1", QEMU_RECV_PORT))
                    except OSError:
                        break
                if inbound_seen:
                    break
                if proc.poll() is not None:
                    break
                time.sleep(0.005)
            stop_sending.set()
            if proc.poll() is None:
                proc.kill()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=5)
            stdout_thread.join(timeout=1)
            stderr_thread.join(timeout=1)
        finally:
            stop_sending.set()
            host_rx.close()
            host_tx.close()
            if proc.poll() is None:
                proc.kill()

    stdout = LOG_PATH.read_text(encoding="utf-8", errors="replace")
    required = [
        "setup.init ok: host and first user created",
        "net.external state=ready scope=outbound-only",
        "net.external bytes=8 data=luna-ext",
        "net.external tx_packets=0->1",
        "net.inbound",
        "net.inbound state=ready scope=external-raw",
        "net.inbound bytes=46",
        "src=02:00:00:00:00:01 dst=52:54:00:12:34:56 ethertype=0x88B6",
        "net.inbound data=luna-inbound-ready............................",
        "net.inbound rx_packets=0->1 filter=dst-mac|broadcast",
        "network.inbound state=ready entry=net.inbound filter=dst-mac|broadcast",
        "network.inbound state=ready entry=net.inbound filter=dst-mac|broadcast event=seen rx_packets=1",
        "net.info tx_packets=1 rx_packets=1",
    ]
    for needle in required:
        if needle not in stdout:
            raise RuntimeError(f"inboundcheck missing expected output: {needle}")

    sys.stdout.write(stdout)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
