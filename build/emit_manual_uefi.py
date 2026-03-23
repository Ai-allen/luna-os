from __future__ import annotations

from pathlib import Path
import struct

from asm import Code


ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "manual_bootx64.efi"

FILE_ALIGN = 0x200
SECTION_ALIGN = 0x1000
IMAGE_BASE = 0

TEXT_RVA = 0x1000
DATA_RVA = 0x2000
RELOC_RVA = 0x3000

TEXT_RAW = 0x200
DATA_RAW = 0x400
RELOC_RAW = 0x600


def align_up(value: int, align: int) -> int:
    return (value + align - 1) & ~(align - 1)


def utf16z(text: str) -> bytes:
    return text.encode("utf-16le") + b"\x00\x00"


def build_text(message_rva: int) -> bytes:
    code = Code(TEXT_RVA)
    code.emit_bytes(b"\x48\x83\xEC\x28")           # sub rsp, 0x28
    code.emit_bytes(b"\x48\x85\xD2")               # test rdx, rdx
    code.emit(0x74)
    code.rel8("fail")
    code.emit_bytes(b"\x48\x8B\x42\x48")           # mov rax, [rdx+0x48]
    code.emit_bytes(b"\x48\x85\xC0")               # test rax, rax
    code.emit(0x74)
    code.rel8("fail")
    code.emit_bytes(b"\x4C\x8B\x40\x08")           # mov r8, [rax+0x08]
    code.emit_bytes(b"\x4D\x85\xC0")               # test r8, r8
    code.emit(0x74)
    code.rel8("fail")
    disp = message_rva - (code.tell() + 7)
    code.emit_bytes(b"\x48\x8D\x15" + struct.pack("<i", disp))  # lea rdx, [rip+msg]
    code.emit_bytes(b"\x48\x89\xC1")               # mov rcx, rax
    code.emit_bytes(b"\x41\xFF\xD0")               # call r8
    code.emit_bytes(b"\x31\xC0")                   # xor eax, eax
    code.emit(0xEB)
    code.rel8("done")
    code.label("fail")
    code.emit_bytes(b"\xB8\x01\x00\x00\x00")       # mov eax, 1
    code.label("done")
    code.emit_bytes(b"\x48\x83\xC4\x28")           # add rsp, 0x28
    code.emit_bytes(b"\xC3")                       # ret
    return code.resolve()


def build_pe() -> bytes:
    anchor_value = DATA_RVA
    message = utf16z("LunaOS UEFI manual\r\n")
    data = struct.pack("<Q", anchor_value) + message
    data = data.ljust(FILE_ALIGN, b"\x00")

    text = build_text(DATA_RVA + 8)
    text = text.ljust(FILE_ALIGN, b"\x90")

    reloc_entries = struct.pack("<HH", 0xA000, 0x0000)
    reloc = struct.pack("<II", DATA_RVA, 8 + len(reloc_entries)) + reloc_entries
    reloc = reloc.ljust(FILE_ALIGN, b"\x00")

    dos = bytearray(0x80)
    dos[0:2] = b"MZ"
    struct.pack_into("<I", dos, 0x3C, 0x80)

    pe = bytearray()
    pe += b"PE\x00\x00"
    pe += struct.pack(
        "<HHIIIHH",
        0x8664,   # Machine x86_64
        3,        # NumberOfSections
        0, 0, 0,
        0xF0,     # SizeOfOptionalHeader
        0x0226,   # Characteristics
    )

    size_of_code = len(text)
    size_of_init = len(data) + len(reloc)
    size_of_image = 0x4000
    size_of_headers = FILE_ALIGN

    opt = bytearray()
    opt += struct.pack("<H", 0x20B)               # PE32+
    opt += struct.pack("<BB", 0, 0)               # linker version
    opt += struct.pack("<III", size_of_code, size_of_init, 0)
    opt += struct.pack("<II", TEXT_RVA, TEXT_RVA)
    opt += struct.pack("<Q", IMAGE_BASE)
    opt += struct.pack("<II", SECTION_ALIGN, FILE_ALIGN)
    opt += struct.pack("<HHHHHH", 0, 0, 0, 0, 5, 2)
    opt += struct.pack("<I", 0)
    opt += struct.pack("<III", size_of_image, size_of_headers, 0)
    opt += struct.pack("<HH", 10, 0x160)          # EFI app, DllCharacteristics
    opt += struct.pack("<QQQQ", 0x200000, 0x1000, 0x100000, 0x1000)
    opt += struct.pack("<II", 0, 16)              # LoaderFlags, NumberOfRvaAndSizes

    data_dirs = [(0, 0)] * 16
    data_dirs[5] = (RELOC_RVA, 12)
    for rva, size in data_dirs:
        opt += struct.pack("<II", rva, size)
    pe += opt

    def section(name: bytes, vsize: int, vaddr: int, rsize: int, rptr: int, chars: int) -> bytes:
        return struct.pack("<8sIIIIIIHHI", name.ljust(8, b"\x00"), vsize, vaddr, rsize, rptr, 0, 0, 0, 0, chars)

    pe += section(b".text", len(build_text(DATA_RVA + 8)), TEXT_RVA, len(text), TEXT_RAW, 0x60000020)
    pe += section(b".data", 8 + len(message), DATA_RVA, len(data), DATA_RAW, 0xC0000040)
    pe += section(b".reloc", 12, RELOC_RVA, len(reloc), RELOC_RAW, 0x42000040)

    headers = (dos + pe).ljust(FILE_ALIGN, b"\x00")
    image = bytearray()
    image += headers
    image += text
    image += data
    image += reloc
    return bytes(image)


def main() -> None:
    OUT.write_bytes(build_pe())
    print(OUT)


if __name__ == "__main__":
    main()
