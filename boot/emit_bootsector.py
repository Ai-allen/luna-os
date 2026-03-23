from __future__ import annotations

import struct


def emit_bootsector(stage_sectors: int) -> bytes:
    b = bytearray()

    def raw(*vals: int) -> None:
        b.extend(vals)

    def raw_bytes(blob: bytes) -> None:
        b.extend(blob)

    def here() -> int:
        return len(b)

    def reserve(count: int) -> int:
        pos = len(b)
        b.extend(b"\x00" * count)
        return pos

    raw(0xFA)  # cli
    raw(0x31, 0xC0)  # xor ax, ax
    raw(0x8E, 0xD8)  # mov ds, ax
    raw(0x8E, 0xC0)  # mov es, ax
    raw(0x8E, 0xD0)  # mov ss, ax
    raw(0xBC, 0x00, 0x7C)  # mov sp, 0x7C00
    raw(0xE4, 0x92)  # in al, 0x92
    raw(0x0C, 0x02)  # or al, 0x02
    raw(0xE6, 0x92)  # out 0x92, al
    raw(0xBE)
    dap_ref = reserve(2)  # mov si, dap
    raw(0xB4, 0x42)  # mov ah, 0x42
    raw(0xCD, 0x13)  # int 0x13
    raw(0x72)
    load_fail = reserve(1)  # jc
    raw(0x31, 0xC0)  # xor ax, ax
    raw(0x8E, 0xC0)  # mov es, ax
    raw(0xBF, 0x00, 0x10)  # mov di, 0x1000
    raw(0xB9, 0x00, 0x18)  # mov cx, 0x1800
    raw(0xF3, 0xAB)  # rep stosw

    def emit_qentry(addr: int, value: int) -> None:
        raw(0x66, 0xC7, 0x06)
        raw_bytes(struct.pack("<H", addr))
        raw_bytes(struct.pack("<I", value & 0xFFFFFFFF))
        raw(0x66, 0xC7, 0x06)
        raw_bytes(struct.pack("<H", addr + 4))
        raw_bytes(struct.pack("<I", (value >> 32) & 0xFFFFFFFF))

    emit_qentry(0x1000, 0x2003)
    emit_qentry(0x2000, 0x3003)
    emit_qentry(0x3000, 0x0083)

    raw(0x0F, 0x01, 0x16)
    gdt_ptr_ref = reserve(2)  # lgdt [gdt_ptr]
    raw(0x0F, 0x20, 0xE0)  # mov eax, cr4
    raw(0x83, 0xC8, 0x20)  # or eax, 0x20
    raw(0x0F, 0x22, 0xE0)  # mov cr4, eax
    raw(0x66, 0xB9)
    raw_bytes(struct.pack("<I", 0xC0000080))
    raw(0x0F, 0x32)  # rdmsr
    raw(0x66, 0x0D)
    raw_bytes(struct.pack("<I", 0x00000100))
    raw(0x0F, 0x30)  # wrmsr
    raw(0x66, 0xB8)
    raw_bytes(struct.pack("<I", 0x1000))
    raw(0x0F, 0x22, 0xD8)  # mov cr3, eax
    raw(0x0F, 0x20, 0xC0)  # mov eax, cr0
    raw(0x66, 0x0D)
    raw_bytes(struct.pack("<I", 0x80000001))
    raw(0x0F, 0x22, 0xC0)  # mov cr0, eax
    raw(0x66, 0xEA)
    raw_bytes(struct.pack("<I", 0x10000))
    raw_bytes(struct.pack("<H", 0x0008))

    fail_label = here()
    raw(0xF4)  # hlt
    raw(0xEB, 0xFD)  # jmp $

    while len(b) % 8:
        raw(0x00)

    gdt_ptr_addr = 0x7C00 + len(b)
    raw_bytes(struct.pack("<H", 24 - 1))
    raw_bytes(struct.pack("<I", 0x7C00 + len(b) + 4))

    raw_bytes(b"\x00" * 8)
    raw_bytes(bytes.fromhex("00000000009A2000"))
    raw_bytes(bytes.fromhex("0000000000920000"))

    dap_addr = 0x7C00 + len(b)
    raw(0x10, 0x00)
    raw_bytes(struct.pack("<H", stage_sectors))
    raw_bytes(struct.pack("<H", 0x0000))
    raw_bytes(struct.pack("<H", 0x1000))
    raw_bytes(struct.pack("<Q", 1))

    b[dap_ref:dap_ref + 2] = struct.pack("<H", dap_addr)
    b[gdt_ptr_ref:gdt_ptr_ref + 2] = struct.pack("<H", gdt_ptr_addr)
    b[load_fail] = (fail_label - (load_fail + 1)) & 0xFF

    if len(b) > 510:
        raise ValueError(f"boot sector overflow: {len(b)} bytes")
    b.extend(b"\x00" * (510 - len(b)))
    b.extend(b"\x55\xAA")
    return bytes(b)
