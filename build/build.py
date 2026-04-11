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
APP_OUT_DIR = BUILD_DIR / "apps"

DATA_META_SECTORS = 25
DATA_OBJECT_CAPACITY = 128
DATA_SLOT_SECTORS = 1
STORE_MAGIC = 0x5346414C
STORE_VERSION = 3
STORE_STATE_CLEAN = 0x434C45414E000001
LUNA_VOLUME_HEALTHY = 1
LUNA_MODE_NORMAL = 1
LUNA_NATIVE_PROFILE_SYSTEM = 0x5359534C
LUNA_NATIVE_PROFILE_DATA = 0x5441444C
LUNA_ACTIVATION_PROVISIONED = 1
LUNA_ACTIVATION_ACTIVE = 3
STORE_RESERVED_WORDS = 55
OBJECT_RECORD_SIZE = 88

DISK_SECTOR_SIZE = 512
DISK_TOTAL_SECTORS = 131072
ESP_START_LBA = 2048
ESP_SECTORS = 16384
LAFS_START_LBA = 18432
SYSTEM_SECTORS = 16384
DATA_SECTORS = 49152
SYSTEM_START_LBA = LAFS_START_LBA
DATA_START_LBA = SYSTEM_START_LBA + SYSTEM_SECTORS
RECOVERY_START_LBA = 83968
RECOVERY_SECTORS = 32768
DATA_STORE_LBA = DATA_START_LBA
SYSTEM_STORE_LBA = SYSTEM_START_LBA
ESP_RESERVED_SECTORS = 1
ESP_FAT_COUNT = 2
ESP_ROOT_ENTRIES = 512
ESP_FAT_SECTORS = 64
ESP_SECTORS_PER_CLUSTER = 1
ESP_ROOT_DIR_SECTORS = (ESP_ROOT_ENTRIES * 32 + DISK_SECTOR_SIZE - 1) // DISK_SECTOR_SIZE
ESP_DATA_START_LBA = ESP_RESERVED_SECTORS + ESP_FAT_COUNT * ESP_FAT_SECTORS + ESP_ROOT_DIR_SECTORS
ESP_STAGE_FILE_CLUSTER = 64
ISO_SECTOR_SIZE = 2048
INSTALL_UUID = uuid.uuid5(uuid.NAMESPACE_URL, "lunaos-ga-2026-04-10-native-governance")
INSTALL_UUID_LOW = INSTALL_UUID.int & ((1 << 64) - 1)
INSTALL_UUID_HIGH = INSTALL_UUID.int >> 64
LUNA_PROTO_VERSION = 0x00000250
LUNA_LA_ABI_MAJOR = 1
LUNA_LA_ABI_MINOR = 0
LUNA_SDK_MAJOR = 1
LUNA_SDK_MINOR = 0
LUNA_PROGRAM_BUNDLE_MAGIC = 0x4C554E42
LUNA_PROGRAM_BUNDLE_VERSION = 2
INTEGRITY_SEED = 0xCBF29CE484222325
SIGNATURE_SEED = 0x9A11D00D55AA7711


def parse_layout(path: Path) -> dict:
    layout = {
        "SPACE": {},
        "APP": {},
        "SCRIPT": {},
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
        elif tag == "SCRIPT":
            layout["SCRIPT"][parts[1]] = {"base": int(parts[2], 16), "size": int(parts[3], 16)}
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


def check_stage_layout(layout: dict, blobs: dict[str, bytes], app_blobs: dict[str, bytes], manifest_blob: bytes) -> None:
    regions: list[tuple[int, int, str]] = []
    for name in ["BOOT", "SECURITY", "DATA", "LIFECYCLE", "SYSTEM", "PROGRAM", "TIME", "DEVICE", "OBSERVE", "NETWORK", "GRAPHICS", "PACKAGE", "UPDATE", "AI", "USER", "DIAG"]:
        base = layout["SPACE"][name]
        regions.append((base, base + len(blobs[name]), f"SPACE {name}"))
    for name in ["GUARD", "HELLO", "FILES", "NOTES", "CONSOLE"]:
        base = layout["APP"][name]
        regions.append((base, base + len(app_blobs[name]), f"APP {name}"))
    regions.append((layout["MANIFEST"], layout["MANIFEST"] + len(manifest_blob), "MANIFEST"))
    session = layout["SCRIPT"]["SESSION"]
    regions.append((session["base"], session["base"] + session["size"], "SCRIPT SESSION"))
    regions.append((layout["BOOTVIEW"], layout["BOOTVIEW"], "BOOTVIEW"))
    regions.sort(key=lambda item: item[0])

    for index in range(len(regions) - 1):
        current_base, current_end, current_name = regions[index]
        next_base, _, next_name = regions[index + 1]
        if current_end > next_base:
            raise RuntimeError(
                f"layout overlap: {current_name} @ 0x{current_base:X}-0x{current_end:X} crosses {next_name} @ 0x{next_base:X}"
            )


def write_map(path: Path, sections: list[tuple[str, dict]]) -> None:
    lines = ["LunaOS Constellation Map", ""]
    for title, labels in sections:
        lines.append(f"[{title}]")
        for name in sorted(labels):
            lines.append(f"{name} = 0x{labels[name]:08X}")
        lines.append("")
    path.write_text("\n".join(lines).rstrip() + "\n")


def generate_layout_constants(layout: dict) -> None:
    (ROOT / "include" / "luna_layout.h").write_text(
        "\n".join(
            [
                "#ifndef LUNA_LAYOUT_H",
                "#define LUNA_LAYOUT_H",
                "",
                f"#define LUNA_MANIFEST_ADDR 0x{layout['MANIFEST']:X}ull",
                f"#define LUNA_BOOTVIEW_ADDR 0x{layout['BOOTVIEW']:X}ull",
                "",
                "#endif",
                "",
            ]
        ),
        encoding="utf-8",
    )
    (ROOT / "include" / "luna_layout.rs").write_text(
        "\n".join(
            [
                f"pub const LUNA_MANIFEST_ADDR: u64 = 0x{layout['MANIFEST']:X};",
                f"pub const LUNA_BOOTVIEW_ADDR: u64 = 0x{layout['BOOTVIEW']:X};",
                "",
            ]
        ),
        encoding="utf-8",
    )


def fold_bytes(seed: int, blob: bytes) -> int:
    for value in blob:
        seed ^= value
        seed = ((seed << 5) | (seed >> 59)) & 0xFFFFFFFFFFFFFFFF
        seed = (seed * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return seed


def pack_luna_app(name: str, app_blob: bytes, entry_offset: int, capability_keys: list[int]) -> bytes:
    name_bytes = name.encode("ascii")
    if len(name_bytes) > 15:
        raise ValueError("app name too long")
    if len(capability_keys) > 4:
        raise ValueError("too many capability keys")
    bundle_id = 0
    source_id = 0
    version = 1
    flags = 0
    abi_major = LUNA_LA_ABI_MAJOR
    abi_minor = LUNA_LA_ABI_MINOR
    sdk_major = LUNA_SDK_MAJOR
    sdk_minor = LUNA_SDK_MINOR
    min_proto = LUNA_PROTO_VERSION
    max_proto = LUNA_PROTO_VERSION
    signer_id = 0
    header_size = struct.calcsize("<II8QIIHHHHII16s4Q")
    payload_integrity = fold_bytes(INTEGRITY_SEED, app_blob)
    signature_material = (
        struct.pack("<16s", name_bytes.ljust(16, b"\x00"))
        + struct.pack(
            "<QQQIIHHHHIIQ4Q",
            bundle_id,
            source_id,
            version,
            len(capability_keys),
            flags,
            abi_major,
            abi_minor,
            sdk_major,
            sdk_minor,
            min_proto,
            max_proto,
            signer_id,
            *(capability_keys + [0] * (4 - len(capability_keys))),
        )
        + app_blob
    )
    signature = fold_bytes(SIGNATURE_SEED, signature_material)
    header = struct.pack(
        "<II8QIIHHHHII16s4Q",
        LUNA_PROGRAM_BUNDLE_MAGIC,
        LUNA_PROGRAM_BUNDLE_VERSION,
        bundle_id,
        source_id,
        version,
        payload_integrity,
        header_size,
        header_size + entry_offset,
        signer_id,
        signature,
        len(capability_keys),
        flags,
        abi_major,
        abi_minor,
        sdk_major,
        sdk_minor,
        min_proto,
        max_proto,
        name_bytes.ljust(16, b"\x00"),
        *(capability_keys + [0] * (4 - len(capability_keys))),
    )
    return header + app_blob


def manifest_entries(layout: dict, labels: dict[str, dict], app_sizes: dict[str, int]) -> list[tuple[str, int]]:
    reserve = layout["RESERVE"]["AI"]
    session = layout["SCRIPT"]["SESSION"]
    return [
        ("security_base", layout["SPACE"]["SECURITY"]),
        ("security_boot_entry", labels["SECURITY"]["security_entry_boot"]),
        ("security_gate_entry", labels["SECURITY"]["security_entry_gate"]),
        ("data_base", layout["SPACE"]["DATA"]),
        ("data_boot_entry", labels["DATA"]["data_entry_boot"]),
        ("data_gate_entry", labels["DATA"]["data_entry_gate"]),
        ("lifecycle_base", layout["SPACE"]["LIFECYCLE"]),
        ("lifecycle_boot_entry", labels["LIFECYCLE"]["lifecycle_entry_boot"]),
        ("lifecycle_gate_entry", labels["LIFECYCLE"]["lifecycle_entry_gate"]),
        ("system_base", layout["SPACE"]["SYSTEM"]),
        ("system_boot_entry", labels["SYSTEM"]["system_entry_boot"]),
        ("system_gate_entry", labels["SYSTEM"]["system_entry_gate"]),
        ("time_base", layout["SPACE"]["TIME"]),
        ("time_boot_entry", labels["TIME"]["time_entry_boot"]),
        ("time_gate_entry", labels["TIME"]["time_entry_gate"]),
        ("device_base", layout["SPACE"]["DEVICE"]),
        ("device_boot_entry", labels["DEVICE"]["device_entry_boot"]),
        ("device_gate_entry", labels["DEVICE"]["device_entry_gate"]),
        ("observe_base", layout["SPACE"]["OBSERVE"]),
        ("observe_boot_entry", labels["OBSERVE"]["observe_entry_boot"]),
        ("observe_gate_entry", labels["OBSERVE"]["observe_entry_gate"]),
        ("network_base", layout["SPACE"]["NETWORK"]),
        ("network_boot_entry", labels["NETWORK"]["network_entry_boot"]),
        ("network_gate_entry", labels["NETWORK"]["network_entry_gate"]),
        ("graphics_base", layout["SPACE"]["GRAPHICS"]),
        ("graphics_boot_entry", labels["GRAPHICS"]["graphics_entry_boot"]),
        ("graphics_gate_entry", labels["GRAPHICS"]["graphics_entry_gate"]),
        ("ai_base", layout["SPACE"]["AI"]),
        ("ai_boot_entry", labels["AI"]["ai_entry_boot"]),
        ("ai_gate_entry", labels["AI"]["ai_entry_gate"]),
        ("package_base", layout["SPACE"]["PACKAGE"]),
        ("package_boot_entry", labels["PACKAGE"]["package_entry_boot"]),
        ("package_gate_entry", labels["PACKAGE"]["package_entry_gate"]),
        ("update_base", layout["SPACE"]["UPDATE"]),
        ("update_boot_entry", labels["UPDATE"]["update_entry_boot"]),
        ("update_gate_entry", labels["UPDATE"]["update_entry_gate"]),
        ("program_base", layout["SPACE"]["PROGRAM"]),
        ("program_boot_entry", labels["PROGRAM"]["program_entry_boot"]),
        ("program_gate_entry", labels["PROGRAM"]["program_entry_gate"]),
        ("user_base", layout["SPACE"]["USER"]),
        ("user_boot_entry", labels["USER"]["user_entry_boot"]),
        ("diag_base", layout["SPACE"]["DIAG"]),
        ("diag_boot_entry", labels["DIAG"]["test_entry_boot"]),
        ("app_hello_base", layout["APP"]["HELLO"]),
        ("app_hello_size", app_sizes["HELLO"]),
        ("bootview_base", layout["BOOTVIEW"]),
        ("security_gate_base", layout["GATE"]["SECURITY"]["base"]),
        ("data_gate_base", layout["GATE"]["DATA"]["base"]),
        ("lifecycle_gate_base", layout["GATE"]["LIFECYCLE"]["base"]),
        ("system_gate_base", layout["GATE"]["SYSTEM"]["base"]),
        ("time_gate_base", layout["GATE"]["TIME"]["base"]),
        ("device_gate_base", layout["GATE"]["DEVICE"]["base"]),
        ("observe_gate_base", layout["GATE"]["OBSERVE"]["base"]),
        ("network_gate_base", layout["GATE"]["NETWORK"]["base"]),
        ("graphics_gate_base", layout["GATE"]["GRAPHICS"]["base"]),
        ("ai_gate_base", layout["GATE"]["AI"]["base"]),
        ("package_gate_base", layout["GATE"]["PACKAGE"]["base"]),
        ("update_gate_base", layout["GATE"]["UPDATE"]["base"]),
        ("program_gate_base", layout["GATE"]["PROGRAM"]["base"]),
        ("data_buffer_base", layout["BUFFER"]["DATA"]["base"]),
        ("data_buffer_size", layout["BUFFER"]["DATA"]["size"]),
        ("list_buffer_base", layout["BUFFER"]["LIST"]["base"]),
        ("list_buffer_size", layout["BUFFER"]["LIST"]["size"]),
        ("reserve_base", reserve["base"]),
        ("reserve_size", reserve["size"]),
        ("data_store_lba", DATA_STORE_LBA),
        ("data_object_capacity", DATA_OBJECT_CAPACITY),
        ("data_slot_sectors", DATA_SLOT_SECTORS),
        ("app_files_base", layout["APP"]["FILES"]),
        ("app_files_size", app_sizes["FILES"]),
        ("app_notes_base", layout["APP"]["NOTES"]),
        ("app_notes_size", app_sizes["NOTES"]),
        ("app_console_base", layout["APP"]["CONSOLE"]),
        ("app_console_size", app_sizes["CONSOLE"]),
        ("app_guard_base", layout["APP"]["GUARD"]),
        ("app_guard_size", app_sizes["GUARD"]),
        ("program_app_write_entry", labels["PROGRAM"]["program_app_write"]),
        ("session_script_base", session["base"]),
        ("session_script_size", session["size"]),
        ("system_store_lba", SYSTEM_STORE_LBA),
        ("install_uuid_low", INSTALL_UUID_LOW),
        ("install_uuid_high", INSTALL_UUID_HIGH),
    ]


def manifest_field_offsets(layout: dict, labels: dict[str, dict], app_sizes: dict[str, int]) -> dict[str, int]:
    return {
        name: 8 + index * 8
        for index, (name, _) in enumerate(manifest_entries(layout, labels, app_sizes))
    }


def build_manifest(layout: dict, labels: dict[str, dict], app_sizes: dict[str, int]) -> bytes:
    entries = manifest_entries(layout, labels, app_sizes)
    return struct.pack(
        "<II" + "Q" * len(entries),
        0x414E554C,
        0x00000250,
        *(value for _, value in entries),
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
        # PE sections with file offset 0 are not backed by real section data here;
        # treat them as zero-filled runtime storage instead of copying the DOS header.
        if raw_size and raw_pointer != 0:
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


def build_c_space_multi(
    sources: list[Path],
    linker: Path,
    obj_prefix: str,
    exe_name: str,
    bin_name: str,
    base: int,
    tools: dict[str, str],
    env: dict[str, str],
) -> tuple[bytes, dict]:
    obj_paths: list[Path] = []
    exe_path = OBJ_DIR / exe_name
    bin_path = OBJ_DIR / bin_name

    for index, source in enumerate(sources):
        obj_path = OBJ_DIR / f"{obj_prefix}_{index}.o"
        obj_paths.append(obj_path)
        run([
            tools["gcc"], "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
            "-fno-asynchronous-unwind-tables", "-fno-unwind-tables", "-mno-red-zone",
            "-O2", "-std=c11", "-Wall", "-Wextra", "-I", str(ROOT / "include"),
            "-c", str(source), "-o", str(obj_path)
        ], env)

    run([
        tools["ld"], "-m", "i386pep", "--image-base", "0x0", "--file-alignment", "16",
        "--section-alignment", "16", "-T", str(linker), "-o", str(exe_path),
        *(str(obj_path) for obj_path in obj_paths)
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


def build_lunaloader(
    *,
    stage_lba: int,
    stage_sectors: int,
    stage_load_addr: int,
    stage_entry_offset: int,
    stage_pd_entry_count: int,
    tools: dict[str, str],
    env: dict[str, str],
) -> bytes:
    out = OBJ_DIR / "lunaloader_stage1.bin"
    run([
        tools["nasm"], "-f", "bin",
        f"-DSTAGE_LBA={stage_lba}",
        f"-DSTAGE_SECTORS={stage_sectors}",
        f"-DSTAGE_LOAD_ADDR=0x{stage_load_addr:X}",
        f"-DSTAGE_ENTRY_OFFSET={stage_entry_offset}",
        f"-DSTAGE_PD_ENTRY_COUNT={stage_pd_entry_count}",
        str(ROOT / "boot" / "lunaloader.asm"),
        "-o", str(out),
    ], env)
    return out.read_bytes()


def build_bootsector(lunaloader_sectors: int, tools: dict[str, str], env: dict[str, str]) -> bytes:
    out = OBJ_DIR / "bootsector.bin"
    run([tools["nasm"], "-f", "bin", f"-DLUNALOADER_SECTORS={lunaloader_sectors}", str(ROOT / "boot" / "bootsector.asm"), "-o", str(out)], env)
    return out.read_bytes()


def guid_bytes(text: str) -> bytes:
    return uuid.UUID(text).bytes_le


def build_uefi_stub(
    stage_relative_lba: int,
    stage_bytes: int,
    stage_load_addr: int,
    boot_entry_offset: int,
    manifest_addr: int,
    scratch_orig_base: int,
    scratch_pages: int,
    scratch_field_offsets: list[int],
    tools: dict[str, str],
    env: dict[str, str],
) -> bytes:
    config = OBJ_DIR / "uefi_loader_config.h"
    config.write_text(
        "\n".join(
            [
                "#ifndef UEFI_LOADER_CONFIG_H",
                "#define UEFI_LOADER_CONFIG_H",
                "",
                f"#define LUNA_STAGE_LBA {stage_relative_lba}ull",
                f"#define LUNA_STAGE_BYTES {stage_bytes}ull",
                f"#define LUNA_STAGE_PAGES {(stage_bytes + 0xFFF) // 0x1000}ull",
                f"#define LUNA_STAGE_LOAD_ADDR 0x{stage_load_addr:X}ull",
                f"#define LUNA_BOOT_ENTRY_OFFSET {boot_entry_offset}ull",
                f"#define LUNA_MANIFEST_ADDR 0x{manifest_addr:X}ull",
                f"#define LUNA_SCRATCH_ORIG_BASE 0x{scratch_orig_base:X}ull",
                f"#define LUNA_SCRATCH_PAGES {scratch_pages}ull",
                f"#define LUNA_SCRATCH_FIELD_COUNT {len(scratch_field_offsets)}u",
                "",
                "static const uint32_t luna_scratch_field_offsets[] = {",
                "    " + ", ".join(str(offset) for offset in scratch_field_offsets),
                "};",
                "",
                "#endif",
                "",
            ]
        ),
        encoding="utf-8",
    )
    obj = OBJ_DIR / "uefi_loader.o"
    out = BUILD_DIR / "manual_bootx64.efi"
    run(
        [
            tools["gcc"],
            "-ffreestanding",
            "-fno-builtin",
            "-fno-stack-protector",
            "-fno-asynchronous-unwind-tables",
            "-fno-unwind-tables",
            "-mno-red-zone",
            "-fshort-wchar",
            "-O2",
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-I",
            str(OBJ_DIR),
            "-c",
            str(ROOT / "build" / "uefi_loader.c"),
            "-o",
            str(obj),
        ],
        env,
    )
    run(
        [
            tools["ld"],
            "-m",
            "i386pep",
            "--subsystem",
            "10",
            "--image-base",
            "0x0",
            "--file-alignment",
            "0x200",
            "--section-alignment",
            "0x1000",
            "-e",
            "efi_main",
            "-o",
            str(out),
            str(obj),
        ],
        env,
    )
    if not out.exists():
        raise RuntimeError(f"UEFI loader build did not produce {out}")
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


def build_fat16_esp(efi_blob: bytes, stage_blob: bytes) -> bytes:
    script_blob = b"FS0:\\EFI\\BOOT\\BOOTX64.EFI\r\n"
    total_sectors = ESP_SECTORS
    sectors_per_cluster = ESP_SECTORS_PER_CLUSTER
    reserved_sectors = ESP_RESERVED_SECTORS
    fat_count = ESP_FAT_COUNT
    root_entries = ESP_ROOT_ENTRIES
    root_dir_sectors = ESP_ROOT_DIR_SECTORS
    fat_sectors = ESP_FAT_SECTORS
    data_start = ESP_DATA_START_LBA
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
    stage_clusters = (len(stage_blob) + DISK_SECTOR_SIZE - 1) // DISK_SECTOR_SIZE
    if file_cluster_start + file_clusters > ESP_STAGE_FILE_CLUSTER:
        raise RuntimeError("BOOTX64.EFI exceeded reserved ESP cluster window before stage payload")

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
    for idx in range(stage_clusters):
        cluster = ESP_STAGE_FILE_CLUSTER + idx
        fat[cluster] = 0xFFFF if idx == stage_clusters - 1 else cluster + 1

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
    root_dir[64:96] = dir_entry(short_name("LUNAOS", "IMG"), 0x20, ESP_STAGE_FILE_CLUSTER, len(stage_blob))
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

    stage_padded = stage_blob + b"\x00" * (stage_clusters * DISK_SECTOR_SIZE - len(stage_blob))
    for idx in range(stage_clusters):
        chunk = stage_padded[idx * DISK_SECTOR_SIZE:(idx + 1) * DISK_SECTOR_SIZE]
        write_cluster(ESP_STAGE_FILE_CLUSTER + idx, chunk)

    return bytes(image)


def crc32_bytes(blob: bytes) -> int:
    return zlib.crc32(blob) & 0xFFFFFFFF


def iso_u16(value: int) -> bytes:
    return struct.pack("<H", value) + struct.pack(">H", value)


def iso_u32(value: int) -> bytes:
    return struct.pack("<I", value) + struct.pack(">I", value)


def iso_dir_record(extent_lba: int, data_size: int, flags: int, name: bytes) -> bytes:
    if len(name) > 255:
        raise ValueError("ISO name too long")
    record = bytearray()
    record.append(0)
    record.append(0)
    record.extend(iso_u32(extent_lba))
    record.extend(iso_u32(data_size))
    record.extend(b"\x00" * 7)
    record.append(flags)
    record.append(0)
    record.append(0)
    record.extend(iso_u16(1))
    record.append(len(name))
    record.extend(name)
    if len(name) % 2 == 0:
        record.append(0)
    record[0] = len(record)
    return bytes(record)


def iso_path_table_record(extent_lba: int, parent_dir: int, name: bytes, big_endian: bool) -> bytes:
    record = bytearray()
    record.append(len(name))
    record.append(0)
    if big_endian:
        record.extend(struct.pack(">I", extent_lba))
        record.extend(struct.pack(">H", parent_dir))
    else:
        record.extend(struct.pack("<I", extent_lba))
        record.extend(struct.pack("<H", parent_dir))
    record.extend(name)
    if len(name) % 2 == 1:
        record.append(0)
    return bytes(record)


def build_uefi_iso(esp_blob: bytes, efi_blob: bytes, stage_blob: bytes) -> bytes:
    boot_image = esp_blob
    boot_image_sectors = (len(boot_image) + ISO_SECTOR_SIZE - 1) // ISO_SECTOR_SIZE
    boot_image_padded = boot_image + b"\x00" * (boot_image_sectors * ISO_SECTOR_SIZE - len(boot_image))
    efi_file = efi_blob + b"\x00" * (((len(efi_blob) + ISO_SECTOR_SIZE - 1) // ISO_SECTOR_SIZE) * ISO_SECTOR_SIZE - len(efi_blob))
    stage_file = stage_blob + b"\x00" * (((len(stage_blob) + ISO_SECTOR_SIZE - 1) // ISO_SECTOR_SIZE) * ISO_SECTOR_SIZE - len(stage_blob))
    efi_file_sectors = len(efi_file) // ISO_SECTOR_SIZE
    stage_file_sectors = len(stage_file) // ISO_SECTOR_SIZE

    system_area_sectors = 16
    pvd_lba = 16
    brvd_lba = 17
    term_lba = 18
    l_path_lba = 19
    m_path_lba = 20
    root_dir_lba = 21
    efi_dir_lba = 22
    boot_dir_lba = 23
    boot_catalog_lba = 24
    boot_image_lba = 25
    efi_file_lba = boot_image_lba + boot_image_sectors
    stage_file_lba = efi_file_lba + efi_file_sectors
    total_sectors = stage_file_lba + stage_file_sectors

    volume = bytearray(total_sectors * ISO_SECTOR_SIZE)

    root_path = iso_path_table_record(root_dir_lba, 1, b"\x00", False)
    efi_path = iso_path_table_record(efi_dir_lba, 1, b"EFI", False)
    boot_path = iso_path_table_record(boot_dir_lba, 2, b"BOOT", False)
    root_path_be = iso_path_table_record(root_dir_lba, 1, b"\x00", True)
    efi_path_be = iso_path_table_record(efi_dir_lba, 1, b"EFI", True)
    boot_path_be = iso_path_table_record(boot_dir_lba, 2, b"BOOT", True)
    path_table = root_path + efi_path + boot_path
    path_table_be = root_path_be + efi_path_be + boot_path_be
    volume[l_path_lba * ISO_SECTOR_SIZE:l_path_lba * ISO_SECTOR_SIZE + len(path_table)] = path_table
    volume[m_path_lba * ISO_SECTOR_SIZE:m_path_lba * ISO_SECTOR_SIZE + len(path_table_be)] = path_table_be

    root_dir = bytearray(ISO_SECTOR_SIZE)
    cursor = 0
    for entry in (
        iso_dir_record(root_dir_lba, ISO_SECTOR_SIZE, 0x02, b"\x00"),
        iso_dir_record(root_dir_lba, ISO_SECTOR_SIZE, 0x02, b"\x01"),
        iso_dir_record(efi_dir_lba, ISO_SECTOR_SIZE, 0x02, b"EFI"),
        iso_dir_record(stage_file_lba, len(stage_blob), 0x00, b"LUNAOS.IMG;1"),
    ):
        root_dir[cursor:cursor + len(entry)] = entry
        cursor += len(entry)
    volume[root_dir_lba * ISO_SECTOR_SIZE:(root_dir_lba + 1) * ISO_SECTOR_SIZE] = root_dir

    efi_dir = bytearray(ISO_SECTOR_SIZE)
    cursor = 0
    for entry in (
        iso_dir_record(efi_dir_lba, ISO_SECTOR_SIZE, 0x02, b"\x00"),
        iso_dir_record(root_dir_lba, ISO_SECTOR_SIZE, 0x02, b"\x01"),
        iso_dir_record(boot_dir_lba, ISO_SECTOR_SIZE, 0x02, b"BOOT"),
    ):
        efi_dir[cursor:cursor + len(entry)] = entry
        cursor += len(entry)
    volume[efi_dir_lba * ISO_SECTOR_SIZE:(efi_dir_lba + 1) * ISO_SECTOR_SIZE] = efi_dir

    boot_dir = bytearray(ISO_SECTOR_SIZE)
    cursor = 0
    for entry in (
        iso_dir_record(boot_dir_lba, ISO_SECTOR_SIZE, 0x02, b"\x00"),
        iso_dir_record(efi_dir_lba, ISO_SECTOR_SIZE, 0x02, b"\x01"),
        iso_dir_record(efi_file_lba, len(efi_blob), 0x00, b"BOOTX64.EFI;1"),
    ):
        boot_dir[cursor:cursor + len(entry)] = entry
        cursor += len(entry)
    volume[boot_dir_lba * ISO_SECTOR_SIZE:(boot_dir_lba + 1) * ISO_SECTOR_SIZE] = boot_dir

    pvd = bytearray(ISO_SECTOR_SIZE)
    pvd[0] = 1
    pvd[1:6] = b"CD001"
    pvd[6] = 1
    pvd[8:40] = b"LUNAOS".ljust(32, b" ")
    pvd[40:72] = b"LUNAOS_INSTALLER".ljust(32, b" ")
    pvd[80:88] = iso_u32(total_sectors)
    pvd[120:124] = iso_u16(1)
    pvd[124:128] = iso_u16(1)
    pvd[128:132] = iso_u16(ISO_SECTOR_SIZE)
    pvd[132:140] = iso_u32(len(path_table))
    pvd[140:144] = struct.pack("<I", l_path_lba)
    pvd[148:152] = struct.pack(">I", m_path_lba)
    pvd[156:156 + 34] = iso_dir_record(root_dir_lba, ISO_SECTOR_SIZE, 0x02, b"\x00")
    pvd[190:318] = b"LunaOS Installer".ljust(128, b" ")
    pvd[318:446] = b"LunaOS".ljust(128, b" ")
    pvd[446:574] = b"LunaOS".ljust(128, b" ")
    pvd[574:702] = b"LunaOS".ljust(128, b" ")
    pvd[702:739] = b"2026032900000000\x00"
    pvd[813] = 1
    volume[pvd_lba * ISO_SECTOR_SIZE:(pvd_lba + 1) * ISO_SECTOR_SIZE] = pvd

    brvd = bytearray(ISO_SECTOR_SIZE)
    brvd[0] = 0
    brvd[1:6] = b"CD001"
    brvd[6] = 1
    brvd[7:39] = b"EL TORITO SPECIFICATION".ljust(32, b" ")
    brvd[71:75] = struct.pack("<I", boot_catalog_lba)
    volume[brvd_lba * ISO_SECTOR_SIZE:(brvd_lba + 1) * ISO_SECTOR_SIZE] = brvd

    term = bytearray(ISO_SECTOR_SIZE)
    term[0] = 255
    term[1:6] = b"CD001"
    term[6] = 1
    volume[term_lba * ISO_SECTOR_SIZE:(term_lba + 1) * ISO_SECTOR_SIZE] = term

    catalog = bytearray(ISO_SECTOR_SIZE)
    validation = bytearray(32)
    validation[0] = 1
    validation[1] = 0xEF
    validation[4:28] = b"LunaOS EFI".ljust(24, b" ")
    validation[30] = 0x55
    validation[31] = 0xAA
    checksum = 0
    for idx in range(0, 32, 2):
        if idx == 28:
            continue
        checksum = (checksum + struct.unpack_from("<H", validation, idx)[0]) & 0xFFFF
    struct.pack_into("<H", validation, 28, (-checksum) & 0xFFFF)
    catalog[0:32] = validation

    section_header = bytearray(32)
    section_header[0] = 0x91
    section_header[1] = 0xEF
    struct.pack_into("<H", section_header, 2, 1)
    section_header[4:32] = b"UEFI".ljust(28, b" ")
    catalog[32:64] = section_header

    section_entry = bytearray(32)
    section_entry[0] = 0x88
    section_entry[1] = 0x00
    struct.pack_into("<H", section_entry, 2, 0)
    section_entry[4] = 0
    section_count = (len(boot_image) + 511) // 512
    struct.pack_into("<H", section_entry, 6, min(section_count, 0xFFFF))
    struct.pack_into("<I", section_entry, 8, boot_image_lba)
    catalog[64:96] = section_entry
    volume[boot_catalog_lba * ISO_SECTOR_SIZE:(boot_catalog_lba + 1) * ISO_SECTOR_SIZE] = catalog

    volume[boot_image_lba * ISO_SECTOR_SIZE:(boot_image_lba + boot_image_sectors) * ISO_SECTOR_SIZE] = boot_image_padded
    volume[efi_file_lba * ISO_SECTOR_SIZE:(efi_file_lba + efi_file_sectors) * ISO_SECTOR_SIZE] = efi_file
    volume[stage_file_lba * ISO_SECTOR_SIZE:(stage_file_lba + stage_file_sectors) * ISO_SECTOR_SIZE] = stage_file
    return bytes(volume)


def fold_bytes(seed: int, blob: bytes) -> int:
    for value in blob:
        seed ^= value
        seed = ((seed << 5) | (seed >> 59)) & 0xFFFFFFFFFFFFFFFF
        seed = (seed * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return seed


def build_native_store_blob(store_base_lba: int, total_sectors: int, profile: int, peer_lba: int, activation_state: int) -> bytes:
    blob = bytearray(total_sectors * DISK_SECTOR_SIZE)
    objects = b"\x00" * (DATA_OBJECT_CAPACITY * OBJECT_RECORD_SIZE)
    reserved = [0] * STORE_RESERVED_WORDS
    reserved[0] = STORE_STATE_CLEAN
    reserved[1] = fold_bytes(0xCBF29CE484222325, objects)
    reserved[2] = 0
    reserved[3] = 0
    reserved[4] = LUNA_VOLUME_HEALTHY
    reserved[5] = LUNA_MODE_NORMAL
    reserved[6] = profile
    reserved[7] = INSTALL_UUID_LOW
    reserved[8] = INSTALL_UUID_HIGH
    reserved[9] = activation_state
    reserved[10] = peer_lba
    superblock = struct.pack(
        "<IIIIQQQQQ" + "Q" * STORE_RESERVED_WORDS,
        STORE_MAGIC,
        STORE_VERSION,
        DATA_OBJECT_CAPACITY,
        DATA_SLOT_SECTORS,
        store_base_lba,
        store_base_lba + 1 + DATA_META_SECTORS - 1,
        1,
        1,
        0,
        *reserved,
    )
    blob[:len(superblock)] = superblock
    object_offset = DISK_SECTOR_SIZE
    blob[object_offset:object_offset + len(objects)] = objects
    return bytes(blob)


def build_gpt_disk(esp_blob: bytes, system_blob: bytes, data_blob: bytes) -> bytes:
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
            "Luna System",
            "A19D880F-05FC-4D3B-A006-743F0F84911E",
            SYSTEM_START_LBA,
            SYSTEM_START_LBA + SYSTEM_SECTORS - 1,
            0,
        ),
        (
            "Luna Data",
            "6A82CB45-1DD2-11B2-A7A6-080020736631",
            DATA_START_LBA,
            DATA_START_LBA + DATA_SECTORS - 1,
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

    system_offset = SYSTEM_START_LBA * DISK_SECTOR_SIZE
    disk[system_offset:system_offset + len(system_blob)] = system_blob
    data_offset = DATA_START_LBA * DISK_SECTOR_SIZE
    disk[data_offset:data_offset + len(data_blob)] = data_blob
    return bytes(disk)


def main() -> None:
    BUILD_DIR.mkdir(exist_ok=True)
    OBJ_DIR.mkdir(exist_ok=True)
    APP_OUT_DIR.mkdir(exist_ok=True)
    layout = parse_layout(BUILD_DIR / "luna.ld")
    generate_layout_constants(layout)
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
        "DIAG": "test",
    }.items():
        if name == "GRAPHICS":
            blobs[name], labels[name] = build_c_space_multi(
                [
                    ROOT / rel / "main.c",
                    ROOT / rel / "fb_backend.c",
                    ROOT / rel / "ui_backend.c",
                    ROOT / rel / "window_store.c",
                    ROOT / rel / "scene_render.c",
                    ROOT / rel / "shell_surfaces.c",
                    ROOT / rel / "hit_test.c",
                    ROOT / rel / "gate.c",
                    ROOT / rel / "ui.c",
                ],
                ROOT / rel / f"{rel}.ld",
                f"{rel}_stage",
                f"{rel}_stage.exe",
                f"{rel}.bin",
                layout["SPACE"][name],
                tools,
                env,
            )
        else:
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

    app_specs = {
        "GUARD": ("guard", "guard_app.c", "guard_app.ld", "guard_app_entry"),
        "HELLO": ("hello", "hello_app.c", "hello_app.ld", "hello_app_entry"),
        "FILES": ("files", "files_app.c", "files_app.ld", "files_app_entry"),
        "NOTES": ("notes", "notes_app.c", "notes_app.ld", "notes_app_entry"),
        "CONSOLE": ("console", "console_app.c", "console_app.ld", "console_app_entry"),
    }
    app_blobs: dict[str, bytes] = {}
    app_sizes: dict[str, int] = {}
    for key, (app_name, source_name, linker_name, entry_name) in app_specs.items():
        app_blob_raw, app_labels = build_c_space(
            ROOT / "apps" / source_name,
            ROOT / "apps" / linker_name,
            f"{app_name}_app_main.o",
            f"{app_name}_app_stage.exe",
            f"{app_name}_app.bin",
            layout["APP"][key],
            tools,
            env,
        )
        labels[f"APP.{key}"] = app_labels
        (APP_OUT_DIR / f"{app_name}.la").write_bytes(app_blob_raw)
        entry_offset = struct.calcsize("<IIQII16s4Q") + (app_labels[entry_name] - layout["APP"][key])
        app_blob = pack_luna_app(app_name, app_blob_raw, entry_offset, [])
        app_blobs[key] = app_blob
        app_sizes[key] = len(app_blob)
        (APP_OUT_DIR / f"{app_name}.luna").write_bytes(app_blob)

    manifest_blob = build_manifest(layout, labels, app_sizes)
    manifest_offsets = manifest_field_offsets(layout, labels, app_sizes)
    check_stage_layout(layout, blobs, app_blobs, manifest_blob)

    stage_base = layout["IMAGE_BASE"]
    stage = bytearray()
    for name in ["BOOT", "SECURITY", "DATA", "LIFECYCLE", "SYSTEM", "PROGRAM", "TIME", "DEVICE", "OBSERVE", "NETWORK", "GRAPHICS", "PACKAGE", "UPDATE", "AI", "USER", "DIAG"]:
        place(stage, stage_base, layout["SPACE"][name], blobs[name])
    for key in ["GUARD", "HELLO", "FILES", "NOTES", "CONSOLE"]:
        place(stage, stage_base, layout["APP"][key], app_blobs[key])
    place(stage, stage_base, layout["SCRIPT"]["SESSION"]["base"], b"\x00" * layout["SCRIPT"]["SESSION"]["size"])
    place(stage, stage_base, layout["MANIFEST"], manifest_blob)

    stage_sectors = (len(stage) + 511) // 512
    stage.extend(b"\x00" * (stage_sectors * 512 - len(stage)))
    stage_blob = bytes(stage)

    bios_boot_entry_offset = labels["BOOT"]["boot_stage_entry"] - layout["SPACE"]["BOOT"]
    stage_pd_entry_count = max(512, (layout["IMAGE_BASE"] + len(stage_blob) + 0x1FFFFF) // 0x200000)
    lunaloader_blob = build_lunaloader(
        stage_lba=2,
        stage_sectors=stage_sectors,
        stage_load_addr=layout["IMAGE_BASE"],
        stage_entry_offset=bios_boot_entry_offset,
        stage_pd_entry_count=stage_pd_entry_count,
        tools=tools,
        env=env,
    )
    lunaloader_sectors = (len(lunaloader_blob) + 511) // 512
    lunaloader_blob = build_lunaloader(
        stage_lba=1 + lunaloader_sectors,
        stage_sectors=stage_sectors,
        stage_load_addr=layout["IMAGE_BASE"],
        stage_entry_offset=bios_boot_entry_offset,
        stage_pd_entry_count=stage_pd_entry_count,
        tools=tools,
        env=env,
    )
    lunaloader_sectors = (len(lunaloader_blob) + 511) // 512
    lunaloader_blob = lunaloader_blob.ljust(lunaloader_sectors * 512, b"\x00")

    system_store_blob = build_native_store_blob(
        SYSTEM_STORE_LBA,
        SYSTEM_SECTORS,
        LUNA_NATIVE_PROFILE_SYSTEM,
        DATA_STORE_LBA,
        LUNA_ACTIVATION_ACTIVE,
    )
    data_store_blob = build_native_store_blob(
        DATA_STORE_LBA,
        DATA_SECTORS,
        LUNA_NATIVE_PROFILE_DATA,
        SYSTEM_STORE_LBA,
        LUNA_ACTIVATION_PROVISIONED,
    )

    image = bytearray(build_bootsector(lunaloader_sectors, tools, env) + lunaloader_blob + stage_blob)
    minimum_size = (DATA_STORE_LBA + DATA_SECTORS) * 512
    if len(image) < minimum_size:
        image.extend(b"\x00" * (minimum_size - len(image)))
    system_offset = SYSTEM_STORE_LBA * DISK_SECTOR_SIZE
    data_offset = DATA_STORE_LBA * DISK_SECTOR_SIZE
    image[system_offset:system_offset + len(system_store_blob)] = system_store_blob
    image[data_offset:data_offset + len(data_store_blob)] = data_store_blob
    (BUILD_DIR / "lunaos.img").write_bytes(bytes(image))

    boot_entry_offset = labels["BOOT"]["boot_uefi_entry"] - layout["SPACE"]["BOOT"]
    stage_relative_lba = ESP_DATA_START_LBA + (ESP_STAGE_FILE_CLUSTER - 2) * ESP_SECTORS_PER_CLUSTER
    scratch_orig_base = layout["BOOTVIEW"]
    scratch_end = max(
        layout["BOOTVIEW"] + 0x1000,
        *(gate["base"] + gate["size"] for gate in layout["GATE"].values()),
        *(buf["base"] + buf["size"] for buf in layout["BUFFER"].values()),
    )
    scratch_pages = (scratch_end - scratch_orig_base + 0xFFF) // 0x1000
    scratch_field_offsets = [
        manifest_offsets["bootview_base"],
        manifest_offsets["security_gate_base"],
        manifest_offsets["data_gate_base"],
        manifest_offsets["lifecycle_gate_base"],
        manifest_offsets["system_gate_base"],
        manifest_offsets["time_gate_base"],
        manifest_offsets["device_gate_base"],
        manifest_offsets["observe_gate_base"],
        manifest_offsets["network_gate_base"],
        manifest_offsets["graphics_gate_base"],
        manifest_offsets["ai_gate_base"],
        manifest_offsets["package_gate_base"],
        manifest_offsets["update_gate_base"],
        manifest_offsets["program_gate_base"],
        manifest_offsets["data_buffer_base"],
        manifest_offsets["list_buffer_base"],
    ]
    uefi_blob = build_uefi_stub(
        stage_relative_lba,
        len(stage_blob),
        layout["IMAGE_BASE"],
        boot_entry_offset,
        layout["MANIFEST"],
        scratch_orig_base,
        scratch_pages,
        scratch_field_offsets,
        tools,
        env,
    )
    esp_blob = build_fat16_esp(uefi_blob, stage_blob)
    gpt_disk = build_gpt_disk(esp_blob, system_store_blob, data_store_blob)
    efi_dir = BUILD_DIR / "EFI" / "BOOT"
    efi_dir.mkdir(parents=True, exist_ok=True)
    (efi_dir / "BOOTX64.EFI").write_bytes(uefi_blob)
    (BUILD_DIR / "lunaos_esp.img").write_bytes(esp_blob)
    (BUILD_DIR / "lunaos_disk.img").write_bytes(gpt_disk)
    (BUILD_DIR / "lunaos_installer.iso").write_bytes(build_uefi_iso(esp_blob, uefi_blob, stage_blob))

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
        ("DIAG", labels["DIAG"]),
        ("APP.HELLO", labels["APP.HELLO"]),
        ("APP.GUARD", labels["APP.GUARD"]),
        ("APP.FILES", labels["APP.FILES"]),
        ("APP.NOTES", labels["APP.NOTES"]),
        ("APP.CONSOLE", labels["APP.CONSOLE"]),
        ("SCRIPT", {"SESSION": layout["SCRIPT"]["SESSION"]["base"]}),
    ])

    print(f"[build] image: {BUILD_DIR / 'lunaos.img'}")
    print(f"[build] uefi: {efi_dir / 'BOOTX64.EFI'}")
    print(f"[build] esp: {BUILD_DIR / 'lunaos_esp.img'}")
    print(f"[build] disk: {BUILD_DIR / 'lunaos_disk.img'}")
    print(f"[build] iso: {BUILD_DIR / 'lunaos_installer.iso'}")
    print(f"[build] sectors: {stage_sectors}")
    for name in ["BOOT", "SECURITY", "DATA", "LIFECYCLE", "SYSTEM", "PROGRAM", "TIME", "DEVICE", "OBSERVE", "NETWORK", "GRAPHICS", "PACKAGE", "UPDATE", "AI", "USER", "DIAG"]:
        print(f"[build] {name.lower()} bytes: {len(blobs[name])}")
    print(f"[build] guard.la bytes: {len((APP_OUT_DIR / 'guard.la').read_bytes())}")
    print(f"[build] guard.luna bytes: {len(app_blobs['GUARD'])}")
    print(f"[build] hello.la bytes: {len((APP_OUT_DIR / 'hello.la').read_bytes())}")
    print(f"[build] hello.luna bytes: {len(app_blobs['HELLO'])}")
    print(f"[build] files.la bytes: {len((APP_OUT_DIR / 'files.la').read_bytes())}")
    print(f"[build] files.luna bytes: {len(app_blobs['FILES'])}")
    print(f"[build] notes.la bytes: {len((APP_OUT_DIR / 'notes.la').read_bytes())}")
    print(f"[build] notes.luna bytes: {len(app_blobs['NOTES'])}")
    print(f"[build] console.la bytes: {len((APP_OUT_DIR / 'console.la').read_bytes())}")
    print(f"[build] console.luna bytes: {len(app_blobs['CONSOLE'])}")
    print(f"[build] manifest @ 0x{layout['MANIFEST']:X}")


if __name__ == "__main__":
    main()
