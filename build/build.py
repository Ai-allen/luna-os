from __future__ import annotations

from pathlib import Path
import os
import shutil
import struct
import subprocess
import sys
import uuid
import zlib


ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = ROOT / "build"
OBJ_DIR = BUILD_DIR / "obj"

DATA_STORE_LBA = 1024
DATA_META_SECTORS = 23
DATA_OBJECT_CAPACITY = 128
DATA_SLOT_SECTORS = 1

DISK_SECTOR_SIZE = 512
DISK_TOTAL_SECTORS = 131072
ESP_START_LBA = 2048
ESP_SECTORS = 16384
LAFS_START_LBA = 18432
LAFS_SECTORS = 65536
RECOVERY_START_LBA = 83968
RECOVERY_SECTORS = 32768


def parse_layout(path: Path) -> dict:
    layout = {
        "SPACE": {},
        "APP": {},
        "STACK": {},
        "RESERVE": {},
        "GATE": {},
        "BUFFER": {},
    }
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        tag = parts[0]
        if tag == "IMAGE_BASE":
            layout["IMAGE_BASE"] = int(parts[1], 16)
        elif tag == "SPACE":
            layout["SPACE"][parts[1]] = int(parts[2], 16)
        elif tag == "APP":
            layout["APP"][parts[1]] = int(parts[2], 16)
        elif tag == "MANIFEST":
            layout["MANIFEST"] = int(parts[1], 16)
        elif tag == "BOOTVIEW":
            layout["BOOTVIEW"] = int(parts[1], 16)
        elif tag == "GATE":
            layout["GATE"][parts[1]] = {"base": int(parts[2], 16), "size": int(parts[3], 16)}
        elif tag == "BUFFER":
            layout["BUFFER"][parts[1]] = {"base": int(parts[2], 16), "size": int(parts[3], 16)}
        elif tag == "STACK":
            layout["STACK"][parts[1]] = int(parts[2], 16)
        elif tag == "RESERVE":
            layout["RESERVE"][parts[1]] = {"base": int(parts[2], 16), "size": int(parts[3], 16)}
        else:
            raise ValueError(f"unknown layout line: {line}")
    return layout


def run(cmd: list[str], env: dict[str, str] | None = None) -> None:
    print(f"[build] {' '.join(cmd)}")
    subprocess.run(cmd, cwd=ROOT, env=env, check=True)


def capture(cmd: list[str], env: dict[str, str] | None = None) -> str:
    return subprocess.check_output(cmd, cwd=ROOT, env=env, text=True)


def place(stage: bytearray, stage_base: int, base: int, blob: bytes) -> None:
    offset = base - stage_base
    end = offset + len(blob)
    if offset < 0:
        raise ValueError("negative placement")
    if end > len(stage):
        stage.extend(b"\x00" * (end - len(stage)))
    stage[offset:end] = blob


def write_map(path: Path, sections: list[tuple[str, dict]]) -> None:
    lines = ["LunaOS Constellation Map", ""]
    for title, labels in sections:
        lines.append(f"[{title}]")
        for name in sorted(labels):
            lines.append(f"{name} = 0x{labels[name]:08X}")
        lines.append("")
    path.write_text("\n".join(lines).rstrip() + "\n")


def pack_luna_app(name: str, app_blob: bytes, entry_offset: int, capability_keys: list[int]) -> bytes:
    name_bytes = name.encode("ascii")
    if len(name_bytes) > 15:
        raise ValueError("app name too long")
    if len(capability_keys) > 4:
        raise ValueError("too many capability keys")
    header = struct.pack(
        "<IIQII16s4Q",
        0x50414E55,
        1,
        entry_offset,
        len(capability_keys),
        0,
        name_bytes.ljust(16, b"\x00"),
        *(capability_keys + [0] * (4 - len(capability_keys))),
    )
    return header + app_blob


def build_manifest(layout: dict, labels: dict[str, dict], hello_app_size: int) -> bytes:
    reserve = layout["RESERVE"]["AI"]
    return struct.pack(
        "<II" + "Q" * 67,
        0x414E554C,
        0x00000250,
        layout["SPACE"]["SECURITY"],
        labels["SECURITY"]["security_entry_boot"],
        labels["SECURITY"]["security_entry_gate"],
        layout["SPACE"]["DATA"],
        labels["DATA"]["data_entry_boot"],
        labels["DATA"]["data_entry_gate"],
        layout["SPACE"]["LIFECYCLE"],
        labels["LIFECYCLE"]["lifecycle_entry_boot"],
        labels["LIFECYCLE"]["lifecycle_entry_gate"],
        layout["SPACE"]["SYSTEM"],
        labels["SYSTEM"]["system_entry_boot"],
        labels["SYSTEM"]["system_entry_gate"],
        layout["SPACE"]["TIME"],
        labels["TIME"]["time_entry_boot"],
        labels["TIME"]["time_entry_gate"],
        layout["SPACE"]["DEVICE"],
        labels["DEVICE"]["device_entry_boot"],
        labels["DEVICE"]["device_entry_gate"],
        layout["SPACE"]["OBSERVE"],
        labels["OBSERVE"]["observe_entry_boot"],
        labels["OBSERVE"]["observe_entry_gate"],
        layout["SPACE"]["NETWORK"],
        labels["NETWORK"]["network_entry_boot"],
        labels["NETWORK"]["network_entry_gate"],
        layout["SPACE"]["GRAPHICS"],
        labels["GRAPHICS"]["graphics_entry_boot"],
        labels["GRAPHICS"]["graphics_entry_gate"],
        layout["SPACE"]["AI"],
        labels["AI"]["ai_entry_boot"],
        labels["AI"]["ai_entry_gate"],
        layout["SPACE"]["PACKAGE"],
        labels["PACKAGE"]["package_entry_boot"],
        labels["PACKAGE"]["package_entry_gate"],
        layout["SPACE"]["UPDATE"],
        labels["UPDATE"]["update_entry_boot"],
        labels["UPDATE"]["update_entry_gate"],
        layout["SPACE"]["PROGRAM"],
        labels["PROGRAM"]["program_entry_boot"],
        labels["PROGRAM"]["program_entry_gate"],
        layout["SPACE"]["USER"],
        labels["USER"]["user_entry_boot"],
        layout["SPACE"]["TEST"],
        labels["TEST"]["test_entry_boot"],
        layout["APP"]["HELLO"],
        hello_app_size,
        layout["GATE"]["SECURITY"]["base"],
        layout["GATE"]["DATA"]["base"],
        layout["GATE"]["LIFECYCLE"]["base"],
        layout["GATE"]["SYSTEM"]["base"],
        layout["GATE"]["TIME"]["base"],
        layout["GATE"]["DEVICE"]["base"],
        layout["GATE"]["OBSERVE"]["base"],
        layout["GATE"]["NETWORK"]["base"],
        layout["GATE"]["GRAPHICS"]["base"],
        layout["GATE"]["AI"]["base"],
        layout["GATE"]["PACKAGE"]["base"],
        layout["GATE"]["UPDATE"]["base"],
        layout["GATE"]["PROGRAM"]["base"],
        layout["BUFFER"]["DATA"]["base"],
        layout["BUFFER"]["DATA"]["size"],
        layout["BUFFER"]["LIST"]["base"],
        layout["BUFFER"]["LIST"]["size"],
        reserve["base"],
        reserve["size"],
        DATA_STORE_LBA,
        DATA_OBJECT_CAPACITY,
        DATA_SLOT_SECTORS,
    )


def default_tool(name: str) -> str | None:
    return shutil.which(name)


def local_tool(*parts: str) -> Path:
    return ROOT.joinpath(*parts)


def detect_toolchain() -> tuple[dict[str, str], dict[str, str]]:
    winlibs_bin = local_tool("toolchains", "winlibs", "mingw64", "bin")
    rust_toolchain = local_tool("toolchains", "rustup-home", "toolchains", "stable-x86_64-pc-windows-gnu")
    rust_bin = rust_toolchain / "bin"
    tools = {
        "gcc": os.environ.get("LUNA_GCC") or default_tool("x86_64-elf-gcc") or str(winlibs_bin / "gcc.exe"),
        "ld": os.environ.get("LUNA_LD") or default_tool("x86_64-elf-ld") or str(winlibs_bin / "ld.exe"),
        "nm": os.environ.get("LUNA_NM") or default_tool("x86_64-elf-nm") or str(winlibs_bin / "nm.exe"),
        "nasm": os.environ.get("LUNA_NASM") or default_tool("nasm") or str(winlibs_bin / "nasm.exe"),
        "rustc": os.environ.get("LUNA_RUSTC") or str(rust_bin / "rustc.exe"),
    }
    for name, tool in tools.items():
        if not Path(tool).exists() and shutil.which(tool) is None:
            raise RuntimeError(f"missing required tool '{name}': {tool}")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([str(winlibs_bin), str(rust_bin), env.get("PATH", "")])
    env["CARGO_HOME"] = str(local_tool("toolchains", "cargo-home"))
    env["RUSTUP_HOME"] = str(local_tool("toolchains", "rustup-home"))
    return tools, env


def parse_nm_symbols(text: str, base: int) -> dict[str, int]:
    labels: dict[str, int] = {}
    for line in text.splitlines():
        parts = line.split()
        if len(parts) < 3:
            continue
        value, kind, name = parts[0], parts[1], parts[2]
        if kind.lower() not in {"t", "r", "d"}:
            continue
        if name.startswith("."):
            continue
        labels[name] = base + int(value, 16)
    return labels


def read_u16(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<H", blob, offset)[0]


def read_u32(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<I", blob, offset)[0]


def pe_section_flat_binary(exe_path: Path) -> bytes:
    blob = exe_path.read_bytes()
    pe_offset = read_u32(blob, 0x3C)
    if blob[pe_offset:pe_offset + 4] != b"PE\x00\x00":
        raise RuntimeError(f"{exe_path} is not a PE image")

    file_header = pe_offset + 4
    number_of_sections = read_u16(blob, file_header + 2)
    size_of_optional_header = read_u16(blob, file_header + 16)
    optional_header = file_header + 20
    section_table = optional_header + size_of_optional_header

    allowed = {b".text", b".rdata", b".data", b".bss"}
    spans: list[tuple[int, int, bytes]] = []
    image_size = 0

    for index in range(number_of_sections):
        section = section_table + index * 40
        raw_name = blob[section:section + 8].rstrip(b"\x00")
        virtual_size = read_u32(blob, section + 8)
        virtual_address = read_u32(blob, section + 12)
        raw_size = read_u32(blob, section + 16)
        raw_pointer = read_u32(blob, section + 20)
        if raw_name not in allowed:
            continue
        span_size = max(virtual_size, raw_size)
        if span_size == 0:
            continue
        image_size = max(image_size, virtual_address + span_size)
        raw = b""
        if raw_size:
            raw = blob[raw_pointer:raw_pointer + raw_size]
        spans.append((virtual_address, span_size, raw))

    if not spans:
        raise RuntimeError(f"{exe_path} contains no exportable sections")

    flat = bytearray(image_size)
    for virtual_address, span_size, raw in spans:
        end = virtual_address + min(len(raw), span_size)
        flat[virtual_address:end] = raw[: span_size]
    return bytes(flat)


def build_c_space(source: Path, linker: Path, obj_name: str, exe_name: str, bin_name: str, base: int, tools: dict[str, str], env: dict[str, str]) -> tuple[bytes, dict]:
    obj_path = OBJ_DIR / obj_name
    exe_path = OBJ_DIR / exe_name
    bin_path = OBJ_DIR / bin_name
    run([
        tools["gcc"], "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
        "-fno-asynchronous-unwind-tables", "-fno-unwind-tables", "-mno-red-zone",
        "-O2", "-std=c11", "-Wall", "-Wextra", "-I", str(ROOT / "include"),
        "-c", str(source), "-o", str(obj_path)
    ], env)
    run([
        tools["ld"], "-m", "i386pep", "--image-base", "0x0", "--file-alignment", "16",
        "--section-alignment", "16", "-T", str(linker), "-o", str(exe_path), str(obj_path)
    ], env)
    bin_path.write_bytes(pe_section_flat_binary(exe_path))
    labels = parse_nm_symbols(capture([tools["nm"], "-n", str(exe_path)], env), base)
    return bin_path.read_bytes(), labels


def build_rust_space(source: Path, linker: Path, crate_name: str, obj_name: str, exe_name: str, bin_name: str, base: int, tools: dict[str, str], env: dict[str, str]) -> tuple[bytes, dict]:
    obj_path = OBJ_DIR / obj_name
    exe_path = OBJ_DIR / exe_name
    bin_path = OBJ_DIR / bin_name
    run([
        tools["rustc"], "--crate-name", crate_name, "--edition=2024", "--target", "x86_64-pc-windows-gnu",
        "-O", "-C", "panic=abort", "--emit", "obj", "-o", str(obj_path), str(source)
    ], env)
    run([
        tools["ld"], "-m", "i386pep", "--image-base", "0x0", "--file-alignment", "16",
        "--section-alignment", "16", "-T", str(linker), "-o", str(exe_path), str(obj_path)
    ], env)
    bin_path.write_bytes(pe_section_flat_binary(exe_path))
    labels = parse_nm_symbols(capture([tools["nm"], "-n", str(exe_path)], env), base)
    return bin_path.read_bytes(), labels


def build_boot(layout: dict, tools: dict[str, str], env: dict[str, str]) -> tuple[bytes, dict]:
    boot_obj = OBJ_DIR / "boot_main.o"
    entry_obj = OBJ_DIR / "boot_entry.o"
    boot_exe = OBJ_DIR / "boot_stage.exe"
    boot_bin = OBJ_DIR / "boot_stage.bin"
    run([
        tools["gcc"], "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
        "-fno-asynchronous-unwind-tables", "-fno-unwind-tables", "-mno-red-zone",
        "-O2", "-std=c11", "-Wall", "-Wextra", "-I", str(ROOT / "include"),
        "-c", str(ROOT / "boot" / "main.c"), "-o", str(boot_obj)
    ], env)
    run([tools["nasm"], "-f", "win64", str(ROOT / "boot" / "entry.asm"), "-o", str(entry_obj)], env)
    run([
        tools["ld"], "-m", "i386pep", "--image-base", "0x0", "--file-alignment", "16",
        "--section-alignment", "16", "-T", str(ROOT / "boot" / "boot.ld"),
        "-o", str(boot_exe), str(entry_obj), str(boot_obj)
    ], env)
    boot_bin.write_bytes(pe_section_flat_binary(boot_exe))
    labels = parse_nm_symbols(capture([tools["nm"], "-n", str(boot_exe)], env), layout["SPACE"]["BOOT"])
    return boot_bin.read_bytes(), labels


def build_bootsector(stage_sectors: int, tools: dict[str, str], env: dict[str, str]) -> bytes:
    out = OBJ_DIR / "bootsector.bin"
    run([tools["nasm"], "-f", "bin", f"-DSTAGE_SECTORS={stage_sectors}", str(ROOT / "boot" / "bootsector.asm"), "-o", str(out)], env)
    return out.read_bytes()


def guid_bytes(text: str) -> bytes:
    return uuid.UUID(text).bytes_le


def build_uefi_stub(tools: dict[str, str], env: dict[str, str]) -> bytes:
    out = BUILD_DIR / "manual_bootx64.efi"
    run([sys.executable, str(ROOT / "build" / "emit_manual_uefi.py")], env)
    if not out.exists():
        raise RuntimeError(f"manual UEFI emitter did not produce {out}")
    target = OBJ_DIR / "BOOTX64-stripped.EFI"
    target.write_bytes(out.read_bytes())
    return target.read_bytes()


def fat_date_time() -> tuple[int, int]:
    return 0x5A58, 0x0000


def short_name(name: str, ext: str = "") -> bytes:
    stem = name.upper().ljust(8)[:8]
    suffix = ext.upper().ljust(3)[:3]
    return (stem + suffix).encode("ascii")


def dir_entry(name83: bytes, attr: int, cluster: int, size: int = 0) -> bytes:
    date, time = fat_date_time()
    return struct.pack(
        "<11sBBBHHHHHHHI",
        name83,
        attr,
        0,
        0,
        time,
        date,
        date,
        (cluster >> 16) & 0xFFFF,
        time,
        date,
        cluster & 0xFFFF,
        size,
    )


def make_directory_sector(self_cluster: int, parent_cluster: int, entries: list[bytes]) -> bytes:
    sector = bytearray(DISK_SECTOR_SIZE)
    cursor = 0
    defaults = [
        dir_entry(short_name(".", ""), 0x10, self_cluster, 0),
        dir_entry(short_name("..", ""), 0x10, parent_cluster, 0),
    ]
    for entry in defaults + entries:
        sector[cursor:cursor + 32] = entry
        cursor += 32
    return bytes(sector)


def build_fat16_esp(efi_blob: bytes) -> bytes:
    script_blob = b"FS0:\\EFI\\BOOT\\BOOTX64.EFI\r\n"
    total_sectors = ESP_SECTORS
    sectors_per_cluster = 1
    reserved_sectors = 1
    fat_count = 2
    root_entries = 512
    root_dir_sectors = (root_entries * 32 + DISK_SECTOR_SIZE - 1) // DISK_SECTOR_SIZE
    fat_sectors = 64
    data_start = reserved_sectors + fat_count * fat_sectors + root_dir_sectors
    cluster_count = total_sectors - data_start
    if cluster_count < 4085:
        raise ValueError("ESP FAT16 cluster count too small")

    volume_id = 0x4C554E41
    boot = bytearray(DISK_SECTOR_SIZE)
    boot[0:3] = b"\xEB\x3C\x90"
    boot[3:11] = b"LUNAFAT "
    struct.pack_into("<H", boot, 11, DISK_SECTOR_SIZE)
    boot[13] = sectors_per_cluster
    struct.pack_into("<H", boot, 14, reserved_sectors)
    boot[16] = fat_count
    struct.pack_into("<H", boot, 17, root_entries)
    if total_sectors < 0x10000:
        struct.pack_into("<H", boot, 19, total_sectors)
    boot[21] = 0xF8
    struct.pack_into("<H", boot, 22, fat_sectors)
    struct.pack_into("<H", boot, 24, 63)
    struct.pack_into("<H", boot, 26, 255)
    struct.pack_into("<I", boot, 28, ESP_START_LBA)
    if total_sectors >= 0x10000:
        struct.pack_into("<I", boot, 32, total_sectors)
    boot[36] = 0x80
    boot[38] = 0x29
    struct.pack_into("<I", boot, 39, volume_id)
    boot[43:54] = b"LUNAESP    "
    boot[54:62] = b"FAT16   "
    boot[510:512] = b"\x55\xAA"

    root_cluster = 2
    boot_dir_cluster = 3
    script_cluster = 4
    file_cluster_start = 5
    script_clusters = (len(script_blob) + DISK_SECTOR_SIZE - 1) // DISK_SECTOR_SIZE
    file_clusters = (len(efi_blob) + DISK_SECTOR_SIZE - 1) // DISK_SECTOR_SIZE

    fat = [0] * (cluster_count + 2)
    fat[0] = 0xFFF8
    fat[1] = 0xFFFF
    fat[root_cluster] = 0xFFFF
    fat[boot_dir_cluster] = 0xFFFF
    for idx in range(script_clusters):
        cluster = script_cluster + idx
        fat[cluster] = 0xFFFF if idx == script_clusters - 1 else cluster + 1
    for idx in range(file_clusters):
        cluster = file_cluster_start + idx
        fat[cluster] = 0xFFFF if idx == file_clusters - 1 else cluster + 1

    image = bytearray(total_sectors * DISK_SECTOR_SIZE)
    image[0:DISK_SECTOR_SIZE] = boot

    fat_blob = bytearray(fat_sectors * DISK_SECTOR_SIZE)
    for idx, value in enumerate(fat[: (len(fat_blob) // 2)]):
        struct.pack_into("<H", fat_blob, idx * 2, value)
    for copy in range(fat_count):
        start = (reserved_sectors + copy * fat_sectors) * DISK_SECTOR_SIZE
        image[start:start + len(fat_blob)] = fat_blob

    root_dir_start = (reserved_sectors + fat_count * fat_sectors) * DISK_SECTOR_SIZE
    root_dir = bytearray(root_dir_sectors * DISK_SECTOR_SIZE)
    root_dir[0:32] = dir_entry(short_name("EFI"), 0x10, root_cluster, 0)
    root_dir[32:64] = dir_entry(short_name("STARTUP", "NSH"), 0x20, script_cluster, len(script_blob))
    image[root_dir_start:root_dir_start + len(root_dir)] = root_dir

    data_region_start = data_start * DISK_SECTOR_SIZE

    def write_cluster(cluster: int, blob: bytes) -> None:
        start = data_region_start + (cluster - 2) * sectors_per_cluster * DISK_SECTOR_SIZE
        image[start:start + len(blob)] = blob

    efi_dir_entries = [dir_entry(short_name("BOOT"), 0x10, boot_dir_cluster, 0)]
    write_cluster(root_cluster, make_directory_sector(root_cluster, 0, efi_dir_entries))

    boot_dir_entries = [dir_entry(short_name("BOOTX64", "EFI"), 0x20, file_cluster_start, len(efi_blob))]
    write_cluster(boot_dir_cluster, make_directory_sector(boot_dir_cluster, root_cluster, boot_dir_entries))

    script_padded = script_blob + b"\x00" * (script_clusters * DISK_SECTOR_SIZE - len(script_blob))
    for idx in range(script_clusters):
        chunk = script_padded[idx * DISK_SECTOR_SIZE:(idx + 1) * DISK_SECTOR_SIZE]
        write_cluster(script_cluster + idx, chunk)

    padded = efi_blob + b"\x00" * (file_clusters * DISK_SECTOR_SIZE - len(efi_blob))
    for idx in range(file_clusters):
        chunk = padded[idx * DISK_SECTOR_SIZE:(idx + 1) * DISK_SECTOR_SIZE]
        write_cluster(file_cluster_start + idx, chunk)

    return bytes(image)


def crc32_bytes(blob: bytes) -> int:
    return zlib.crc32(blob) & 0xFFFFFFFF


def build_gpt_disk(esp_blob: bytes, lafs_blob: bytes) -> bytes:
    total_sectors = DISK_TOTAL_SECTORS
    disk = bytearray(total_sectors * DISK_SECTOR_SIZE)

    disk[0:440] = b"LunaOS GPT skeleton".ljust(440, b"\x00")
    struct.pack_into("<I", disk, 440, 0)
    struct.pack_into("<H", disk, 444, 0)
    disk[446:462] = struct.pack(
        "<B3sB3sII",
        0x00,
        b"\x00\x02\x00",
        0xEE,
        b"\xFF\xFF\xFF",
        1,
        min(total_sectors - 1, 0xFFFFFFFF),
    )
    disk[510:512] = b"\x55\xAA"

    entries = bytearray(128 * 128)
    disk_guid = uuid.uuid4()
    partitions = [
        (
            "EFI System",
            "C12A7328-F81F-11D2-BA4B-00A0C93EC93B",
            ESP_START_LBA,
            ESP_START_LBA + ESP_SECTORS - 1,
            0,
        ),
        (
            "LAFS Data",
            "A19D880F-05FC-4D3B-A006-743F0F84911E",
            LAFS_START_LBA,
            LAFS_START_LBA + LAFS_SECTORS - 1,
            0,
        ),
        (
            "Luna Recovery",
            "6A898CC3-1DD2-11B2-99A6-080020736631",
            RECOVERY_START_LBA,
            RECOVERY_START_LBA + RECOVERY_SECTORS - 1,
            0,
        ),
    ]
    for index, (name, type_guid, first_lba, last_lba, attrs) in enumerate(partitions):
        part_guid = uuid.uuid4().bytes_le
        name_utf16 = name.encode("utf-16le").ljust(72, b"\x00")
        entry = struct.pack(
            "<16s16sQQQ72s",
            guid_bytes(type_guid),
            part_guid,
            first_lba,
            last_lba,
            attrs,
            name_utf16,
        )
        start = index * 128
        entries[start:start + 128] = entry

    entries_lba = 2
    entries_sectors = len(entries) // DISK_SECTOR_SIZE
    entries_crc = crc32_bytes(entries)
    disk[entries_lba * DISK_SECTOR_SIZE:(entries_lba + entries_sectors) * DISK_SECTOR_SIZE] = entries

    def write_gpt_header(current_lba: int, backup_lba: int, entries_start_lba: int, entries_blob: bytes) -> bytes:
        header = bytearray(DISK_SECTOR_SIZE)
        header[0:8] = b"EFI PART"
        struct.pack_into("<I", header, 8, 0x00010000)
        struct.pack_into("<I", header, 12, 92)
        struct.pack_into("<Q", header, 24, current_lba)
        struct.pack_into("<Q", header, 32, backup_lba)
        struct.pack_into("<Q", header, 40, 34)
        struct.pack_into("<Q", header, 48, total_sectors - 34)
        header[56:72] = disk_guid.bytes_le
        struct.pack_into("<Q", header, 72, entries_start_lba)
        struct.pack_into("<I", header, 80, 128)
        struct.pack_into("<I", header, 84, 128)
        struct.pack_into("<I", header, 88, crc32_bytes(entries_blob))
        struct.pack_into("<I", header, 16, 0)
        struct.pack_into("<I", header, 16, crc32_bytes(header[:92]))
        return bytes(header)

    primary_header = write_gpt_header(1, total_sectors - 1, entries_lba, entries)
    disk[DISK_SECTOR_SIZE:2 * DISK_SECTOR_SIZE] = primary_header

    backup_entries_lba = total_sectors - 33
    disk[backup_entries_lba * DISK_SECTOR_SIZE:(backup_entries_lba + entries_sectors) * DISK_SECTOR_SIZE] = entries
    backup_header = write_gpt_header(total_sectors - 1, 1, backup_entries_lba, entries)
    disk[(total_sectors - 1) * DISK_SECTOR_SIZE:total_sectors * DISK_SECTOR_SIZE] = backup_header

    esp_offset = ESP_START_LBA * DISK_SECTOR_SIZE
    disk[esp_offset:esp_offset + len(esp_blob)] = esp_blob

    lafs_offset = LAFS_START_LBA * DISK_SECTOR_SIZE
    disk[lafs_offset:lafs_offset + len(lafs_blob)] = lafs_blob
    return bytes(disk)


def main() -> None:
    BUILD_DIR.mkdir(exist_ok=True)
    OBJ_DIR.mkdir(exist_ok=True)
    layout = parse_layout(BUILD_DIR / "luna.ld")
    tools, env = detect_toolchain()

    labels: dict[str, dict] = {}
    blobs: dict[str, bytes] = {}

    blobs["BOOT"], labels["BOOT"] = build_boot(layout, tools, env)
    blobs["SECURITY"], labels["SECURITY"] = build_rust_space(ROOT / "security" / "src" / "main.rs", ROOT / "security" / "security.ld", "luna_security", "security_main.o", "security_stage.exe", "security.bin", layout["SPACE"]["SECURITY"], tools, env)
    blobs["DATA"], labels["DATA"] = build_rust_space(ROOT / "data" / "src" / "main.rs", ROOT / "data" / "data.ld", "luna_data", "data_main.o", "data_stage.exe", "data.bin", layout["SPACE"]["DATA"], tools, env)
    blobs["LIFECYCLE"], labels["LIFECYCLE"] = build_rust_space(ROOT / "lifecycle" / "src" / "main.rs", ROOT / "lifecycle" / "lifecycle.ld", "luna_lifecycle", "lifecycle_main.o", "lifecycle_stage.exe", "lifecycle.bin", layout["SPACE"]["LIFECYCLE"], tools, env)
    blobs["SYSTEM"], labels["SYSTEM"] = build_rust_space(ROOT / "system" / "src" / "main.rs", ROOT / "system" / "system.ld", "luna_system", "system_main.o", "system_stage.exe", "system.bin", layout["SPACE"]["SYSTEM"], tools, env)

    for name, rel in {
        "PROGRAM": "program",
        "TIME": "time",
        "DEVICE": "device",
        "OBSERVE": "observe",
        "NETWORK": "network",
        "GRAPHICS": "graphics",
        "PACKAGE": "package",
        "UPDATE": "update",
        "AI": "ai",
        "USER": "user",
        "TEST": "test",
    }.items():
        blobs[name], labels[name] = build_c_space(
            ROOT / rel / "main.c",
            ROOT / rel / f"{rel}.ld",
            f"{rel}_main.o",
            f"{rel}_stage.exe",
            f"{rel}.bin",
            layout["SPACE"][name],
            tools,
            env,
        )

    hello_blob_raw, hello_labels = build_c_space(
        ROOT / "apps" / "hello_app.c",
        ROOT / "apps" / "hello_app.ld",
        "hello_app_main.o",
        "hello_app_stage.exe",
        "hello_app.bin",
        layout["APP"]["HELLO"],
        tools,
        env,
    )
    labels["APP.HELLO"] = hello_labels
    hello_entry_offset = struct.calcsize("<IIQII16s4Q") + (hello_labels["hello_app_entry"] - layout["APP"]["HELLO"])
    hello_app_blob = pack_luna_app("hello", hello_blob_raw, hello_entry_offset, [])
    manifest_blob = build_manifest(layout, labels, len(hello_app_blob))

    stage_base = layout["IMAGE_BASE"]
    stage = bytearray()
    for name in ["BOOT", "SECURITY", "DATA", "LIFECYCLE", "SYSTEM", "PROGRAM", "TIME", "DEVICE", "OBSERVE", "NETWORK", "GRAPHICS", "PACKAGE", "UPDATE", "AI", "USER", "TEST"]:
        place(stage, stage_base, layout["SPACE"][name], blobs[name])
    place(stage, stage_base, layout["APP"]["HELLO"], hello_app_blob)
    place(stage, stage_base, layout["MANIFEST"], manifest_blob)

    stage_sectors = (len(stage) + 511) // 512
    stage.extend(b"\x00" * (stage_sectors * 512 - len(stage)))
    image = build_bootsector(stage_sectors, tools, env) + bytes(stage)
    minimum_size = (DATA_STORE_LBA + DATA_META_SECTORS + DATA_OBJECT_CAPACITY * DATA_SLOT_SECTORS) * 512
    if len(image) < minimum_size:
        image += b"\x00" * (minimum_size - len(image))
    (BUILD_DIR / "lunaos.img").write_bytes(image)

    uefi_blob = build_uefi_stub(tools, env)
    esp_blob = build_fat16_esp(uefi_blob)
    gpt_disk = build_gpt_disk(esp_blob, image[: min(len(image), LAFS_SECTORS * DISK_SECTOR_SIZE)])
    efi_dir = BUILD_DIR / "EFI" / "BOOT"
    efi_dir.mkdir(parents=True, exist_ok=True)
    (efi_dir / "BOOTX64.EFI").write_bytes(uefi_blob)
    (BUILD_DIR / "lunaos_esp.img").write_bytes(esp_blob)
    (BUILD_DIR / "lunaos_disk.img").write_bytes(gpt_disk)

    write_map(BUILD_DIR / "constellation.map", [
        ("BOOT", labels["BOOT"]),
        ("SECURITY", labels["SECURITY"]),
        ("DATA", labels["DATA"]),
        ("LIFECYCLE", labels["LIFECYCLE"]),
        ("SYSTEM", labels["SYSTEM"]),
        ("PROGRAM", labels["PROGRAM"]),
        ("TIME", labels["TIME"]),
        ("DEVICE", labels["DEVICE"]),
        ("OBSERVE", labels["OBSERVE"]),
        ("NETWORK", labels["NETWORK"]),
        ("GRAPHICS", labels["GRAPHICS"]),
        ("PACKAGE", labels["PACKAGE"]),
        ("UPDATE", labels["UPDATE"]),
        ("AI", labels["AI"]),
        ("USER", labels["USER"]),
        ("TEST", labels["TEST"]),
        ("APP.HELLO", labels["APP.HELLO"]),
    ])

    print(f"[build] image: {BUILD_DIR / 'lunaos.img'}")
    print(f"[build] uefi: {efi_dir / 'BOOTX64.EFI'}")
    print(f"[build] esp: {BUILD_DIR / 'lunaos_esp.img'}")
    print(f"[build] disk: {BUILD_DIR / 'lunaos_disk.img'}")
    print(f"[build] sectors: {stage_sectors}")
    for name in ["BOOT", "SECURITY", "DATA", "LIFECYCLE", "SYSTEM", "PROGRAM", "TIME", "DEVICE", "OBSERVE", "NETWORK", "GRAPHICS", "PACKAGE", "UPDATE", "AI", "USER", "TEST"]:
        print(f"[build] {name.lower()} bytes: {len(blobs[name])}")
    print(f"[build] hello.luna bytes: {len(hello_app_blob)}")
    print(f"[build] manifest @ 0x{layout['MANIFEST']:X}")


if __name__ == "__main__":
    main()
