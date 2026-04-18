from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SOURCE = ROOT / "build" / "lunaos_disk.img"
SECTOR_SIZE = 512
GPT_HEADER_LBA = 1
GPT_ENTRY_SIZE = 128
GPT_ENTRY_COUNT = 128
EXPECTED_PARTITIONS = ("EFI System", "Luna System", "Luna Data", "Luna Recovery")
EXPECTED_ESP_FILES = {"EFI", "STARTUP.NSH", "LUNAOS.IMG"}
STORE_MAGIC = 0x5346414C
STORE_VERSION = 3
LUNA_NATIVE_PROFILE_SYSTEM = 0x5359534C
LUNA_NATIVE_PROFILE_DATA = 0x5441444C


@dataclass
class TargetSpec:
    kind: str
    value: str
    size_bytes: int
    display_name: str


@dataclass
class PartitionRecord:
    index: int
    name: str
    first_lba: int
    last_lba: int
    type_guid: str

    @property
    def size_bytes(self) -> int:
        return (self.last_lba - self.first_lba + 1) * SECTOR_SIZE


@dataclass
class NativeStoreRecord:
    magic: int
    version: int
    object_capacity: int
    slot_sectors: int
    store_base_lba: int
    data_start_lba: int
    next_nonce: int
    format_count: int
    mount_count: int
    reserved: tuple[int, ...]

    @property
    def profile(self) -> int:
        return self.reserved[6]

    @property
    def install_uuid_low(self) -> int:
        return self.reserved[7]

    @property
    def install_uuid_high(self) -> int:
        return self.reserved[8]

    @property
    def activation(self) -> int:
        return self.reserved[9]

    @property
    def peer_lba(self) -> int:
        return self.reserved[10]


def is_windows_admin() -> bool:
    if os.name != "nt":
        return False
    try:
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except Exception:
        return False


def format_bytes(value: int) -> str:
    units = ["B", "KiB", "MiB", "GiB", "TiB"]
    size = float(value)
    idx = 0
    while size >= 1024.0 and idx + 1 < len(units):
        size /= 1024.0
        idx += 1
    return f"{size:.2f} {units[idx]}"


def guid_from_bytes(blob: bytes) -> str:
    d1, d2, d3, d4 = struct.unpack("<IHH8s", blob[:16])
    tail = d4.hex()
    return f"{d1:08X}-{d2:04X}-{d3:04X}-{tail[:4].upper()}-{tail[4:].upper()}"


def parse_gpt(blob: bytes) -> list[PartitionRecord]:
    if len(blob) < SECTOR_SIZE * 34:
        raise RuntimeError("target is too small to contain GPT")
    header = blob[GPT_HEADER_LBA * SECTOR_SIZE:(GPT_HEADER_LBA + 1) * SECTOR_SIZE]
    if header[:8] != b"EFI PART":
        raise RuntimeError("missing GPT header")
    entries_lba = struct.unpack_from("<Q", header, 72)[0]
    entry_count = struct.unpack_from("<I", header, 80)[0]
    entry_size = struct.unpack_from("<I", header, 84)[0]
    if entry_size != GPT_ENTRY_SIZE:
        raise RuntimeError(f"unexpected GPT entry size: {entry_size}")
    records: list[PartitionRecord] = []
    entries = blob[entries_lba * SECTOR_SIZE:(entries_lba * SECTOR_SIZE) + entry_count * entry_size]
    for index in range(entry_count):
        entry = entries[index * entry_size:(index + 1) * entry_size]
        if entry[:16] == b"\x00" * 16:
            continue
        first_lba, last_lba = struct.unpack_from("<QQ", entry, 32)
        name = entry[56:128].decode("utf-16le", errors="ignore").rstrip("\x00")
        records.append(
            PartitionRecord(
                index=index,
                name=name,
                first_lba=first_lba,
                last_lba=last_lba,
                type_guid=guid_from_bytes(entry[:16]),
            )
        )
    return records


def parse_store(blob: bytes, start_lba: int) -> NativeStoreRecord:
    offset = start_lba * SECTOR_SIZE
    superblock = blob[offset:offset + SECTOR_SIZE]
    fmt = "<IIIIQQQQQ" + "Q" * 55
    words = struct.unpack(fmt, superblock[:struct.calcsize(fmt)])
    return NativeStoreRecord(
        magic=words[0],
        version=words[1],
        object_capacity=words[2],
        slot_sectors=words[3],
        store_base_lba=words[4],
        data_start_lba=words[5],
        next_nonce=words[6],
        format_count=words[7],
        mount_count=words[8],
        reserved=tuple(words[9:]),
    )


def parse_fat_name(raw: bytes) -> str:
    stem = raw[:8].decode("ascii", errors="ignore").rstrip(" ")
    ext = raw[8:11].decode("ascii", errors="ignore").rstrip(" ")
    return stem if not ext else f"{stem}.{ext}"


def verify_esp(blob: bytes, start_lba: int, sector_count: int) -> None:
    offset = start_lba * SECTOR_SIZE
    esp = blob[offset:offset + sector_count * SECTOR_SIZE]
    if len(esp) < SECTOR_SIZE:
        raise RuntimeError("ESP partition is truncated")
    boot = esp[:SECTOR_SIZE]
    if boot[510:512] != b"\x55\xAA":
        raise RuntimeError("ESP boot sector signature mismatch")
    if boot[3:11] != b"LUNAFAT ":
        raise RuntimeError("ESP OEM identifier mismatch")
    reserved = struct.unpack_from("<H", boot, 14)[0]
    fat_count = boot[16]
    root_entries = struct.unpack_from("<H", boot, 17)[0]
    fat_sectors = struct.unpack_from("<H", boot, 22)[0]
    root_dir_sectors = (root_entries * 32 + SECTOR_SIZE - 1) // SECTOR_SIZE
    root_dir_start = (reserved + fat_count * fat_sectors) * SECTOR_SIZE
    root_dir_end = root_dir_start + root_dir_sectors * SECTOR_SIZE
    root_dir = esp[root_dir_start:root_dir_end]
    names: set[str] = set()
    for index in range(0, len(root_dir), 32):
        entry = root_dir[index:index + 32]
        if len(entry) < 32 or entry[0] in (0x00, 0xE5):
            continue
        if entry[11] == 0x0F:
            continue
        names.add(parse_fat_name(entry[:11]))
    missing = sorted(EXPECTED_ESP_FILES - names)
    if missing:
        raise RuntimeError(f"ESP directory entries missing: {', '.join(missing)}")


def read_source_image(path: Path) -> bytes:
    if not path.exists():
        raise RuntimeError(f"source image not found: {path}")
    return path.read_bytes()


def inspect_source(path: Path) -> tuple[bytes, list[PartitionRecord], NativeStoreRecord, NativeStoreRecord]:
    blob = read_source_image(path)
    parts = parse_gpt(blob)
    system = next((part for part in parts if part.name == "Luna System"), None)
    data = next((part for part in parts if part.name == "Luna Data"), None)
    if system is None or data is None:
        raise RuntimeError("source image is missing Luna System or Luna Data")
    system_store = parse_store(blob, system.first_lba)
    data_store = parse_store(blob, data.first_lba)
    return blob, parts, system_store, data_store


def find_powershell_host() -> str:
    for candidate in ("pwsh", "powershell"):
        found = shutil.which(candidate)
        if found:
            return found
    if os.name == "nt":
        win_ps = Path(os.environ.get("WINDIR", r"C:\Windows")) / "System32" / "WindowsPowerShell" / "v1.0" / "powershell.exe"
        if win_ps.exists():
            return str(win_ps)
    raise RuntimeError("PowerShell host not found")


def powershell_json(command: str) -> object:
    try:
        completed = subprocess.run(
            [
                find_powershell_host(),
                "-NoProfile",
                "-Command",
                command,
            ],
            cwd=ROOT,
            check=True,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    except subprocess.CalledProcessError as exc:
        detail = (exc.stderr or exc.stdout or "").strip()
        if "拒绝访问" in detail or "Access is denied" in detail:
            raise RuntimeError("physical-disk enumeration requires an elevated administrator session") from None
        raise RuntimeError(detail or "PowerShell disk query failed") from None
    stdout = completed.stdout.strip()
    return json.loads(stdout) if stdout else []


def list_windows_disks() -> list[dict[str, object]]:
    if os.name != "nt":
        return []
    script = (
        "try { "
        "Get-Disk -ErrorAction Stop | Select-Object Number,FriendlyName,BusType,PartitionStyle,Size,IsBoot,IsSystem,IsOffline,IsReadOnly "
        "| ConvertTo-Json -Depth 3 -Compress "
        "} catch { "
        "Get-CimInstance Win32_DiskDrive -ErrorAction Stop | Select-Object @{Name='Number';Expression={$_.Index}},"
        "@{Name='FriendlyName';Expression={$_.Model}},"
        "@{Name='BusType';Expression={$_.InterfaceType}},"
        "@{Name='PartitionStyle';Expression={'?'}},"
        "Size,@{Name='IsBoot';Expression={$false}},@{Name='IsSystem';Expression={$false}},"
        "@{Name='IsOffline';Expression={$false}},@{Name='IsReadOnly';Expression={$false}} "
        "| ConvertTo-Json -Depth 3 -Compress }"
    )
    raw = powershell_json(script)
    if isinstance(raw, dict):
        return [raw]
    return list(raw)


def target_from_args(args: argparse.Namespace, source_size: int) -> TargetSpec:
    if args.target_image:
        path = Path(args.target_image).resolve()
        size_bytes = path.stat().st_size if path.exists() else 0
        return TargetSpec("image", str(path), size_bytes, str(path))
    if args.disk_number is None:
        raise RuntimeError("target is required")
    records = list_windows_disks()
    for record in records:
        if int(record["Number"]) == args.disk_number:
            size_bytes = int(record.get("Size") or 0)
            return TargetSpec(
                "disk",
                str(args.disk_number),
                size_bytes,
                f"PhysicalDrive{args.disk_number} ({record.get('FriendlyName', 'disk')})",
            )
    return TargetSpec("disk", str(args.disk_number), 0, f"PhysicalDrive{args.disk_number}")


def compute_sha256_stream(stream: BinaryIO, length: int) -> str:
    hasher = hashlib.sha256()
    remaining = length
    while remaining > 0:
        chunk = stream.read(min(4 * 1024 * 1024, remaining))
        if not chunk:
            raise RuntimeError("unexpected end of stream during hashing")
        hasher.update(chunk)
        remaining -= len(chunk)
    return hasher.hexdigest()


def copy_stream(src: BinaryIO, dst: BinaryIO, total_size: int) -> None:
    copied = 0
    while copied < total_size:
        chunk = src.read(min(4 * 1024 * 1024, total_size - copied))
        if not chunk:
            raise RuntimeError("unexpected end of source during install write")
        dst.write(chunk)
        copied += len(chunk)
    dst.flush()


def open_disk_path(disk_number: int, mode: str) -> BinaryIO:
    device = fr"\\.\PhysicalDrive{disk_number}"
    return open(device, mode, buffering=0)


def verify_native_layout(blob: bytes) -> tuple[list[PartitionRecord], NativeStoreRecord, NativeStoreRecord]:
    parts = parse_gpt(blob)
    names = [part.name for part in parts]
    for expected in EXPECTED_PARTITIONS:
        if expected not in names:
            raise RuntimeError(f"missing partition: {expected}")
    esp_part = next(part for part in parts if part.name == "EFI System")
    system_part = next(part for part in parts if part.name == "Luna System")
    data_part = next(part for part in parts if part.name == "Luna Data")
    verify_esp(blob, esp_part.first_lba, esp_part.last_lba - esp_part.first_lba + 1)
    system_store = parse_store(blob, system_part.first_lba)
    data_store = parse_store(blob, data_part.first_lba)
    if system_store.magic != STORE_MAGIC or data_store.magic != STORE_MAGIC:
        raise RuntimeError("native store superblock magic mismatch")
    if system_store.version != STORE_VERSION or data_store.version != STORE_VERSION:
        raise RuntimeError("native store version mismatch")
    if system_store.profile != LUNA_NATIVE_PROFILE_SYSTEM:
        raise RuntimeError("LSYS profile mismatch")
    if data_store.profile != LUNA_NATIVE_PROFILE_DATA:
        raise RuntimeError("LDAT profile mismatch")
    if system_store.install_uuid_low != data_store.install_uuid_low or system_store.install_uuid_high != data_store.install_uuid_high:
        raise RuntimeError("LSYS/LDAT install identity mismatch")
    if system_store.peer_lba != data_part.first_lba or data_store.peer_lba != system_part.first_lba:
        raise RuntimeError("LSYS/LDAT peer LBA contract mismatch")
    return parts, system_store, data_store


def print_plan(source_path: Path, target: TargetSpec, partitions: list[PartitionRecord], system_store: NativeStoreRecord, data_store: NativeStoreRecord) -> None:
    print(f"installer.source={source_path}")
    print(f"installer.target={target.display_name}")
    print("installer.layout=ESP+LSYS+LDAT+LRCV")
    for part in partitions:
        print(
            f"installer.partition name={part.name} "
            f"lba={part.first_lba}-{part.last_lba} size={format_bytes(part.size_bytes)}"
        )
    install_uuid = f"{system_store.install_uuid_high:016x}{system_store.install_uuid_low:016x}"
    print(f"installer.identity install_uuid={install_uuid}")
    print(f"installer.system profile=LSYS peer_lba={system_store.peer_lba} activation={system_store.activation}")
    print(f"installer.data profile=LDAT peer_lba={data_store.peer_lba} activation={data_store.activation}")


def command_list(_args: argparse.Namespace) -> int:
    records = list_windows_disks()
    if not records:
        print("installer.disks unavailable")
        return 0
    for record in records:
        print(
            "installer.disk "
            f"number={record.get('Number')} "
            f"name={record.get('FriendlyName')} "
            f"bus={record.get('BusType')} "
            f"size={format_bytes(int(record.get('Size') or 0))} "
            f"boot={str(bool(record.get('IsBoot'))).lower()} "
            f"system={str(bool(record.get('IsSystem'))).lower()}"
        )
    return 0


def command_plan(args: argparse.Namespace) -> int:
    source, parts, system_store, data_store = inspect_source(Path(args.source))
    target = target_from_args(args, len(source))
    if target.size_bytes and target.size_bytes < len(source):
        raise RuntimeError("target is smaller than the required install image")
    print_plan(Path(args.source), target, parts, system_store, data_store)
    return 0


def command_verify(args: argparse.Namespace) -> int:
    if args.target_image:
        blob = Path(args.target_image).read_bytes()
        target_name = str(Path(args.target_image).resolve())
    else:
        if os.name != "nt":
            raise RuntimeError("physical-disk verification is only supported on Windows")
        with open_disk_path(args.disk_number, "rb") as stream:
            blob = stream.read()
        target_name = f"PhysicalDrive{args.disk_number}"
    parts, system_store, data_store = verify_native_layout(blob)
    print(f"installer.verify target={target_name} result=ok")
    for part in parts:
        print(f"installer.verify partition={part.name} lba={part.first_lba}-{part.last_lba}")
    print(
        "installer.verify stores="
        f"LSYS(profile=0x{system_store.profile:08x},peer_lba={system_store.peer_lba}) "
        f"LDAT(profile=0x{data_store.profile:08x},peer_lba={data_store.peer_lba})"
    )
    return 0


def command_apply(args: argparse.Namespace) -> int:
    source_path = Path(args.source)
    source_blob, parts, system_store, data_store = inspect_source(source_path)
    source_size = len(source_blob)
    target = target_from_args(args, source_size)
    if target.size_bytes and target.size_bytes < source_size:
        raise RuntimeError("target is smaller than the required install image")
    if not args.yes:
        raise RuntimeError("apply requires --yes")
    if args.confirm_target and args.confirm_target != target.display_name:
        raise RuntimeError("confirmation target does not match the selected install target")

    print_plan(source_path, target, parts, system_store, data_store)
    print("installer.apply state=writing")

    source_hash = hashlib.sha256(source_blob).hexdigest()
    if target.kind == "image":
        target_path = Path(target.value)
        target_path.parent.mkdir(parents=True, exist_ok=True)
        with source_path.open("rb") as src, target_path.open("wb") as dst:
            copy_stream(src, dst, source_size)
        with target_path.open("rb") as stream:
            target_hash = compute_sha256_stream(stream, source_size)
        blob = target_path.read_bytes()
    else:
        if os.name != "nt":
            raise RuntimeError("physical-disk install is only supported on Windows")
        if not is_windows_admin():
            raise RuntimeError("physical-disk install requires an elevated administrator session")
        disk_number = int(target.value)
        device_path = fr"\\.\PhysicalDrive{disk_number}"
        with source_path.open("rb") as src, open_disk_path(disk_number, "r+b") as dst:
            copy_stream(src, dst, source_size)
        with open_disk_path(disk_number, "rb") as stream:
            target_hash = compute_sha256_stream(stream, source_size)
        with open_disk_path(disk_number, "rb") as stream:
            blob = stream.read(source_size)
        print(f"installer.apply device={device_path}")

    if source_hash != target_hash:
        raise RuntimeError("post-write checksum mismatch")
    verify_native_layout(blob)
    print(f"installer.apply source_sha256={source_hash}")
    print(f"installer.apply target_sha256={target_hash}")
    print("installer.apply result=verified")
    print("installer.next boot=first-setup expected=required")
    return 0


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="LunaOS native installer entry")
    parser.add_argument("--source", default=str(DEFAULT_SOURCE), help="source LunaOS native disk image")
    subparsers = parser.add_subparsers(dest="command", required=True)

    list_parser = subparsers.add_parser("list", help="list install targets")
    list_parser.set_defaults(func=command_list)

    plan_parser = subparsers.add_parser("plan", help="show install plan")
    plan_target = plan_parser.add_mutually_exclusive_group(required=True)
    plan_target.add_argument("--target-image", help="target raw image path")
    plan_target.add_argument("--disk-number", type=int, help="target physical disk number")
    plan_parser.set_defaults(func=command_plan)

    verify_parser = subparsers.add_parser("verify", help="verify a written install target")
    verify_target = verify_parser.add_mutually_exclusive_group(required=True)
    verify_target.add_argument("--target-image", help="target raw image path")
    verify_target.add_argument("--disk-number", type=int, help="target physical disk number")
    verify_parser.set_defaults(func=command_verify)

    apply_parser = subparsers.add_parser("apply", help="apply LunaOS native install to a target")
    apply_target = apply_parser.add_mutually_exclusive_group(required=True)
    apply_target.add_argument("--target-image", help="target raw image path")
    apply_target.add_argument("--disk-number", type=int, help="target physical disk number")
    apply_parser.add_argument("--yes", action="store_true", help="confirm destructive target write")
    apply_parser.add_argument("--confirm-target", help="optional exact target display string confirmation")
    apply_parser.set_defaults(func=command_apply)

    return parser


def main() -> int:
    parser = build_arg_parser()
    args = parser.parse_args()
    return int(args.func(args))


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
