from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, List, Tuple
import struct


def u8(value: int) -> bytes:
    return struct.pack("<B", value & 0xFF)


def u16(value: int) -> bytes:
    return struct.pack("<H", value & 0xFFFF)


def u32(value: int) -> bytes:
    return struct.pack("<I", value & 0xFFFFFFFF)


def u64(value: int) -> bytes:
    return struct.pack("<Q", value & 0xFFFFFFFFFFFFFFFF)


@dataclass
class Code:
    base: int
    buf: bytearray = field(default_factory=bytearray)
    labels: Dict[str, int] = field(default_factory=dict)
    fixups: List[Tuple[int, str, int, int]] = field(default_factory=list)

    def tell(self) -> int:
        return self.base + len(self.buf)

    def offset(self) -> int:
        return len(self.buf)

    def emit(self, *values: int) -> None:
        self.buf.extend(values)

    def emit_bytes(self, blob: bytes) -> None:
        self.buf.extend(blob)

    def emit_u16(self, value: int) -> None:
        self.buf.extend(u16(value))

    def emit_u32(self, value: int) -> None:
        self.buf.extend(u32(value))

    def emit_u64(self, value: int) -> None:
        self.buf.extend(u64(value))

    def label(self, name: str) -> None:
        self.labels[name] = self.tell()

    def align(self, alignment: int, fill: int = 0) -> None:
        while len(self.buf) % alignment:
            self.buf.append(fill & 0xFF)

    def rel8(self, label: str) -> None:
        pos = self.tell()
        self.buf.append(0)
        self.fixups.append((pos, label, 1, pos + 1))

    def rel32(self, label: str) -> None:
        pos = self.tell()
        self.buf.extend(b"\x00\x00\x00\x00")
        self.fixups.append((pos, label, 4, pos + 4))

    def rip32(self, label: str) -> None:
        self.rel32(label)

    def resolve(self) -> bytes:
        out = bytearray(self.buf)
        for pos, label, width, ref in self.fixups:
            target = self.labels[label]
            delta = target - ref
            patch_at = pos - self.base
            if width == 1:
                if not -128 <= delta <= 127:
                    raise ValueError(f"rel8 overflow for {label}: {delta}")
                out[patch_at] = delta & 0xFF
            elif width == 4:
                out[patch_at:patch_at + 4] = struct.pack("<i", delta)
            else:
                raise ValueError(width)
        return bytes(out)

