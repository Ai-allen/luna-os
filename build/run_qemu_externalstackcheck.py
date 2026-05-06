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
CHECK_IMAGE = ROOT / "lunaos_externalstackcheck.img"
LOG_PATH = ROOT / "qemu_externalstackcheck.log"
ERR_PATH = ROOT / "qemu_externalstackcheck.err.log"
BOOT_SECTOR_BYTES = 512
SESSION_MAGIC = 0x54504353
QEMU_SEND_PORT = 41101
QEMU_RECV_PORT = 41102
ETHERTYPE = 0x88B5
LUNALINK_MAGIC = 0x4C4C504B


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
    CHECK_IMAGE.write_bytes(blob)


def parse_outbound_frame(frame: bytes) -> tuple[int, int, int, bytes]:
    if len(frame) < 14 + 20:
        raise RuntimeError("externalstack outbound frame too short")
    ethertype = struct.unpack(">H", frame[12:14])[0]
    if ethertype != ETHERTYPE:
        raise RuntimeError(f"externalstack unexpected ethertype: 0x{ethertype:04X}")
    magic, peer_id, session_id, channel_id, payload_size = struct.unpack("<IIIII", frame[14:34])
    if magic != LUNALINK_MAGIC:
        raise RuntimeError("externalstack unexpected lunalink magic")
    payload = frame[34:34 + payload_size]
    return peer_id, session_id, channel_id, payload


def build_reply_frame(peer_id: int, session_id: int, channel_id: int, payload: bytes) -> bytes:
    packet = struct.pack("<IIIII", LUNALINK_MAGIC, peer_id, session_id, channel_id, len(payload))
    return (
        bytes.fromhex("52 54 00 12 34 56")
        + bytes.fromhex("02 00 00 00 00 01")
        + struct.pack(">H", ETHERTYPE)
        + packet
        + payload
    )


def main() -> int:
    qemu = find_qemu()
    commands = (
        b"setup.init luna dev secret\r\n"
        b"net.connect\r\n"
        b"net.status\r\n"
        b"net.send luna-stack-out\r\n"
        b"net.recv\r\n"
        b"net.status\r\n"
        b"revoke-cap network.send\r\n"
        b"net.send denied-link\r\n"
        b"lasql.logs\r\n"
    )
    patch_session_script(commands)
    if LOG_PATH.exists():
        LOG_PATH.unlink()
    if ERR_PATH.exists():
        ERR_PATH.unlink()

    host_rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    host_rx.bind(("127.0.0.1", QEMU_SEND_PORT))
    host_rx.settimeout(0.05)
    host_tx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    with LOG_PATH.open("w", encoding="utf-8", errors="replace") as stdout_file, ERR_PATH.open("w", encoding="utf-8", errors="replace") as stderr_file:
        stdout_chunks: list[str] = []
        stderr_chunks: list[str] = []
        stdout_lock = threading.Lock()
        stderr_lock = threading.Lock()

        proc = subprocess.Popen(
            [
                qemu,
                "-drive",
                f"format=raw,file={CHECK_IMAGE}",
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

        def pump(stream, sink_file, sink_chunks: list[str], sink_lock: threading.Lock) -> None:
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
            outbound_seen = False
            reply_sent = False
            deadline = time.time() + 30.0
            while time.time() < deadline:
                with stdout_lock:
                    text = "".join(stdout_chunks)
                if not outbound_seen and "net.send state=ready bytes=" in text:
                    try:
                        frame, _ = host_rx.recvfrom(2048)
                        peer_id, session_id, channel_id, payload = parse_outbound_frame(frame)
                        if payload != b"luna-stack-out":
                            raise RuntimeError("externalstack unexpected outbound payload")
                        outbound_seen = True
                        reply = build_reply_frame(peer_id, session_id, channel_id, b"luna-stack-in")
                        for _ in range(16):
                            host_tx.sendto(reply, ("127.0.0.1", QEMU_RECV_PORT))
                            time.sleep(0.01)
                        reply_sent = True
                    except OSError:
                        pass
                if reply_sent and "lasql.logs ok" in text:
                    break
                if proc.poll() is not None:
                    break
                time.sleep(0.02)
            if proc.poll() is None:
                proc.kill()
            proc.wait(timeout=5)
            stdout_thread.join(timeout=1)
            stderr_thread.join(timeout=1)
        finally:
            host_rx.close()
            host_tx.close()
            if proc.poll() is None:
                proc.kill()

    stdout = LOG_PATH.read_text(encoding="utf-8", errors="replace")
    required = [
        "setup.init ok: host and first user created",
        "net.connect state=ready scope=external-message",
        "net.status state=ready transport=external-message peer=external",
        "net.status phase=connect last=ok tx_messages=0 tx_bytes=0 rx_messages=0 rx_bytes=0",
        "net.send state=ready bytes=14",
        "data=luna-stack-out",
        "net.recv state=ready bytes=13",
        "data=luna-stack-in",
        "net.status phase=recv last=ok tx_messages=1 tx_bytes=14 rx_messages=1 rx_bytes=13",
        "revoked: network.send (1)",
        "net.send failed state=invalid_cap",
        "link.trace type=session type=4C534F52 state=1 version=65536",
        "link.trace type=channel type=4C534F52 state=1 version=65536",
        "link.trace type=send type=4C534F52 state=1 version=65536",
        "link.trace type=recv type=4C534F52 state=1 version=65536",
        "link.trace type=send denied type=4C534F52 state=8 version=65536",
    ]
    for needle in required:
        if needle not in stdout:
            raise RuntimeError(f"externalstackcheck missing expected output: {needle}")

    sys.stdout.write(stdout)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
