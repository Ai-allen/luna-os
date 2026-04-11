from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

LUNA_PROGRAM_BUNDLE_MAGIC = 0x4C554E42
LUNA_PROGRAM_BUNDLE_VERSION = 2
LUNA_PROGRAM_BUNDLE_VERSION_LEGACY = 1
LUNA_PROTO_VERSION = 0x00000250
LUNA_LA_ABI_MAJOR = 1
LUNA_LA_ABI_MINOR = 0
LUNA_SDK_MAJOR = 1
LUNA_SDK_MINOR = 0
INTEGRITY_SEED = 0xCBF29CE484222325
SIGNATURE_SEED = 0x9A11D00D55AA7711


def fold_bytes(seed: int, blob: bytes) -> int:
    for value in blob:
        seed ^= value
        seed = ((seed << 5) | (seed >> 59)) & 0xFFFFFFFFFFFFFFFF
        seed = (seed * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return seed


def capability_list(values: list[int]) -> list[int]:
    return values + [0] * (4 - len(values))


def build_signature_material(
    *,
    name: str,
    payload: bytes,
    bundle_id: int,
    source_id: int,
    version: int,
    flags: int,
    capability_keys: list[int],
    abi_major: int,
    abi_minor: int,
    sdk_major: int,
    sdk_minor: int,
    min_proto: int,
    max_proto: int,
    signer_id: int,
) -> bytes:
    return (
        struct.pack("<16s", name.encode("ascii").ljust(16, b"\x00"))
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
            *capability_list(capability_keys),
        )
        + payload
    )


def build_bundle(
    *,
    name: str,
    payload: bytes,
    entry_offset: int,
    bundle_id: int,
    source_id: int,
    version: int,
    flags: int,
    capability_keys: list[int],
    abi_major: int,
    abi_minor: int,
    sdk_major: int,
    sdk_minor: int,
    min_proto: int,
    max_proto: int,
    signer_id: int,
) -> bytes:
    if len(name.encode("ascii")) > 15:
        raise ValueError("name must be 15 ASCII bytes or fewer")
    if len(capability_keys) > 4:
        raise ValueError("at most 4 capability keys are supported")
    header_size = (
        struct.calcsize("<II")
        + struct.calcsize("<8Q")
        + struct.calcsize("<IIHHHHII")
        + struct.calcsize("<16s")
        + struct.calcsize("<4Q")
    )
    bundle_entry_offset = header_size + entry_offset
    integrity = fold_bytes(INTEGRITY_SEED, payload)
    signature = fold_bytes(
        SIGNATURE_SEED ^ ((source_id & 0xFFFFFFFFFFFFFFFF) << 1) ^ signer_id,
        build_signature_material(
            name=name,
            payload=payload,
            bundle_id=bundle_id,
            source_id=source_id,
            version=version,
            flags=flags,
            capability_keys=capability_keys,
            abi_major=abi_major,
            abi_minor=abi_minor,
            sdk_major=sdk_major,
            sdk_minor=sdk_minor,
            min_proto=min_proto,
            max_proto=max_proto,
            signer_id=signer_id,
        ),
    )
    bundle = (
        struct.pack(
            "<II",
            LUNA_PROGRAM_BUNDLE_MAGIC,
            LUNA_PROGRAM_BUNDLE_VERSION,
        )
        + struct.pack(
            "<8Q",
            bundle_id,
            source_id,
            version,
            integrity,
            header_size,
            bundle_entry_offset,
            signer_id,
            signature,
        )
        + struct.pack(
            "<IIHHHHII",
            len(capability_keys),
            flags,
            abi_major,
            abi_minor,
            sdk_major,
            sdk_minor,
            min_proto,
            max_proto,
        )
        + struct.pack(
            "<16s",
            name.encode("ascii").ljust(16, b"\x00"),
        )
        + struct.pack(
            "<4Q",
            *capability_list(capability_keys),
        )
    )
    return bundle + payload


def parse_bundle(blob: bytes) -> dict:
    if len(blob) < 8:
        raise ValueError("bundle too small")
    magic, version = struct.unpack_from("<II", blob, 0)
    if magic != LUNA_PROGRAM_BUNDLE_MAGIC:
        raise ValueError("not a .luna bundle")
    if version == LUNA_PROGRAM_BUNDLE_VERSION_LEGACY:
        if len(blob) < struct.calcsize("<II6QII16s4Q"):
            raise ValueError("legacy bundle truncated")
        (
            _magic,
            _version,
            bundle_id,
            source_id,
            app_version,
            integrity_check,
            header_bytes,
            entry_offset,
            capability_count,
            flags,
            raw_name,
            *caps,
        ) = struct.unpack_from("<II6QII16s4Q", blob, 0)
        payload = blob[header_bytes:]
        return {
            "version": version,
            "name": raw_name.split(b"\x00", 1)[0].decode("ascii"),
            "bundle_id": bundle_id,
            "source_id": source_id,
            "app_version": app_version,
            "integrity_check": integrity_check,
            "header_bytes": header_bytes,
            "entry_offset": entry_offset,
            "flags": flags,
            "capability_count": capability_count,
            "capabilities": list(caps[:capability_count]),
            "abi_major": 0,
            "abi_minor": 0,
            "sdk_major": 0,
            "sdk_minor": 0,
            "min_proto": 0,
            "max_proto": 0,
            "signer_id": 0,
            "signature_check": 0,
            "payload_bytes": len(payload),
        }
    if version != LUNA_PROGRAM_BUNDLE_VERSION:
        raise ValueError(f"unsupported bundle version: {version}")
    if len(blob) < struct.calcsize("<II8QIIHHHHII16s4Q"):
        raise ValueError("bundle truncated")
    (
        _magic,
        _version,
        bundle_id,
        source_id,
        app_version,
        integrity_check,
        header_bytes,
        entry_offset,
        signer_id,
        signature_check,
        capability_count,
        flags,
        abi_major,
        abi_minor,
        sdk_major,
        sdk_minor,
        min_proto,
        max_proto,
        raw_name,
        *caps,
    ) = struct.unpack_from("<II8QIIHHHHII16s4Q", blob, 0)
    payload = blob[header_bytes:]
    return {
        "version": version,
        "name": raw_name.split(b"\x00", 1)[0].decode("ascii"),
        "bundle_id": bundle_id,
        "source_id": source_id,
        "app_version": app_version,
        "integrity_check": integrity_check,
        "header_bytes": header_bytes,
        "entry_offset": entry_offset,
        "flags": flags,
        "capability_count": capability_count,
        "capabilities": list(caps[:capability_count]),
        "abi_major": abi_major,
        "abi_minor": abi_minor,
        "sdk_major": sdk_major,
        "sdk_minor": sdk_minor,
        "min_proto": min_proto,
        "max_proto": max_proto,
        "signer_id": signer_id,
        "signature_check": signature_check,
        "payload_bytes": len(payload),
    }


def parse_manifest(path: Path) -> dict:
    manifest = json.loads(path.read_text(encoding="utf-8"))
    required_fields = ("name", "source_id", "version", "entry_offset")
    for field in required_fields:
        if field not in manifest:
            raise ValueError(f"manifest missing required field: {field}")
    if not isinstance(manifest["name"], str) or not manifest["name"]:
        raise ValueError("manifest field 'name' must be a non-empty string")
    try:
        manifest["name"].encode("ascii")
    except UnicodeEncodeError as exc:
        raise ValueError("manifest field 'name' must be ASCII") from exc
    if len(manifest["name"].encode("ascii")) > 15:
        raise ValueError("manifest field 'name' must be 15 ASCII bytes or fewer")
    for field in ("source_id", "version", "entry_offset"):
        if not isinstance(manifest[field], int):
            raise ValueError(f"manifest field '{field}' must be an integer")
    if manifest["source_id"] < 0 or manifest["version"] < 0 or manifest["entry_offset"] < 0:
        raise ValueError("manifest numeric fields must be non-negative")
    if not isinstance(manifest.get("capabilities", []), list):
        raise ValueError("manifest field 'capabilities' must be a list")
    if len(manifest.get("capabilities", [])) > 4:
        raise ValueError("manifest field 'capabilities' supports at most 4 entries")
    manifest.setdefault("bundle_id", 0)
    manifest.setdefault("flags", 0)
    manifest.setdefault("capabilities", [])
    manifest.setdefault("abi_major", LUNA_LA_ABI_MAJOR)
    manifest.setdefault("abi_minor", LUNA_LA_ABI_MINOR)
    manifest.setdefault("sdk_major", LUNA_SDK_MAJOR)
    manifest.setdefault("sdk_minor", LUNA_SDK_MINOR)
    manifest.setdefault("min_proto", LUNA_PROTO_VERSION)
    manifest.setdefault("max_proto", LUNA_PROTO_VERSION)
    manifest.setdefault("signer_id", 0)
    for field in ("bundle_id", "flags", "abi_major", "abi_minor", "sdk_major", "sdk_minor", "min_proto", "max_proto", "signer_id"):
        if not isinstance(manifest[field], int):
            raise ValueError(f"manifest field '{field}' must be an integer")
    if manifest["source_id"] != 0 and manifest["signer_id"] == 0:
        raise ValueError("manifest field 'signer_id' must be non-zero when 'source_id' is non-zero")
    return manifest


def pack_main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Pack a LunaOS .luna bundle")
    parser.add_argument("manifest", type=Path, help="JSON manifest with name/version/source_id/payload entry_offset/capabilities")
    parser.add_argument("payload", type=Path, help="Raw app binary payload")
    parser.add_argument("output", type=Path, help="Output .luna path")
    args = parser.parse_args(argv)

    manifest = parse_manifest(args.manifest)
    payload = args.payload.read_bytes()
    bundle = build_bundle(
        name=manifest["name"],
        payload=payload,
        entry_offset=int(manifest["entry_offset"]),
        bundle_id=int(manifest["bundle_id"]),
        source_id=int(manifest["source_id"]),
        version=int(manifest["version"]),
        flags=int(manifest["flags"]),
        capability_keys=[int(value, 0) if isinstance(value, str) else int(value) for value in manifest["capabilities"]],
        abi_major=int(manifest["abi_major"]),
        abi_minor=int(manifest["abi_minor"]),
        sdk_major=int(manifest["sdk_major"]),
        sdk_minor=int(manifest["sdk_minor"]),
        min_proto=int(manifest["min_proto"]),
        max_proto=int(manifest["max_proto"]),
        signer_id=int(manifest["signer_id"]),
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(bundle)
    print(f"packed {args.output} ({len(bundle)} bytes)")
    return 0


def inspect_main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Inspect a LunaOS .luna bundle")
    parser.add_argument("bundle", type=Path, help="Input .luna path")
    args = parser.parse_args(argv)
    print(json.dumps(parse_bundle(args.bundle.read_bytes()), indent=2, sort_keys=True))
    return 0


def main() -> int:
    if len(sys.argv) >= 2 and sys.argv[1] == "inspect":
        return inspect_main(sys.argv[2:])
    return pack_main(sys.argv[1:])


if __name__ == "__main__":
    raise SystemExit(main())
