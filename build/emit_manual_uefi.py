from __future__ import annotations

import argparse
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

LOADED_IMAGE_PROTOCOL_GUID = "5B1B31A1-9562-11D2-8E3F-00A0C969723B"
BLOCK_IO_PROTOCOL_GUID = "964E5B21-6459-11D2-8E39-00A0C969723B"


def align_up(value: int, align: int) -> int:
    return (value + align - 1) & ~(align - 1)


def utf16z(text: str) -> bytes:
    return text.encode("utf-16le") + b"\x00\x00"


def guid_bytes(text: str) -> bytes:
    hex_text = text.replace("-", "")
    a = int(hex_text[0:8], 16)
    b = int(hex_text[8:12], 16)
    c = int(hex_text[12:16], 16)
    tail = bytes.fromhex(hex_text[16:])
    return struct.pack("<IHH", a, b, c) + tail


def emit_jz(code: Code, label: str) -> None:
    code.emit_bytes(b"\x0F\x84")
    code.rel32(label)


def emit_js(code: Code, label: str) -> None:
    code.emit_bytes(b"\x0F\x88")
    code.rel32(label)


def emit_jae(code: Code, label: str) -> None:
    code.emit_bytes(b"\x0F\x83")
    code.rel32(label)


def build_text(
    message_rva: int,
    fail1_rva: int,
    fail2_rva: int,
    fail3_rva: int,
    fail4_rva: int,
    fail5_rva: int,
    loaded_guid_rva: int,
    block_guid_rva: int,
    loaded_ptr_rva: int,
    block_ptr_rva: int,
    load_addr_rva: int,
    stage_pages_rva: int,
    manifest_offset_rva: int,
    scratch_orig_base_rva: int,
    scratch_alloc_base_rva: int,
    scratch_pages_rva: int,
    scratch_field_count_rva: int,
    scratch_field_offsets_rva: int,
    stage_lba: int,
    stage_bytes: int,
    boot_entry_offset: int,
) -> bytes:
    code = Code(TEXT_RVA)

    code.emit_bytes(b"\x41\x54")                   # push r12
    code.emit_bytes(b"\x41\x55")                   # push r13
    code.emit_bytes(b"\x53")                       # push rbx
    code.emit_bytes(b"\x56")                       # push rsi
    code.emit_bytes(b"\x57")                       # push rdi
    code.emit_bytes(b"\x48\x83\xEC\x20")           # sub rsp, 0x20
    code.emit_bytes(b"\x48\x89\xCF")               # mov rdi, rcx
    code.emit_bytes(b"\x49\x89\xCC")               # mov r12, rcx
    code.emit_bytes(b"\x48\x89\xCB")               # mov rbx, rcx
    code.emit_bytes(b"\x49\x89\xD5")               # mov r13, rdx
    code.emit_bytes(b"\x4D\x85\xED")               # test r13, r13
    emit_jz(code, "fail1")
    code.emit_bytes(b"\x49\x8B\x45\x40")           # mov rax, [r13+0x40]
    code.emit_bytes(b"\x48\x85\xC0")               # test rax, rax
    emit_jz(code, "fail1")
    code.emit_bytes(b"\x4C\x8B\x40\x08")           # mov r8, [rax+0x08]
    code.emit_bytes(b"\x4D\x85\xC0")               # test r8, r8
    emit_jz(code, "fail1")
    disp = message_rva - (code.tell() + 7)
    code.emit_bytes(b"\x48\x8D\x15" + struct.pack("<i", disp))  # lea rdx, [rip+msg]
    code.emit_bytes(b"\x48\x89\xC1")               # mov rcx, rax
    code.emit_bytes(b"\x41\xFF\xD0")               # call r8

    code.emit_bytes(b"\x49\x8B\x75\x58")           # mov rsi, [r13+0x58]
    code.emit_bytes(b"\x48\x85\xF6")               # test rsi, rsi
    emit_jz(code, "bootsvc_60")
    code.emit_bytes(b"\x48\x8B\x86\x28\x00\x00\x00")  # mov rax, [rsi+0x28]
    code.emit_bytes(b"\x48\x3D\x00\x00\x10\x00")  # cmp rax, 0x100000
    emit_jae(code, "bootsvc_found")
    code.label("bootsvc_60")
    code.emit_bytes(b"\x49\x8B\x75\x60")           # mov rsi, [r13+0x60]
    code.emit_bytes(b"\x48\x85\xF6")               # test rsi, rsi
    emit_jz(code, "bootsvc_68")
    code.emit_bytes(b"\x48\x8B\x86\x28\x00\x00\x00")
    code.emit_bytes(b"\x48\x3D\x00\x00\x10\x00")
    emit_jae(code, "bootsvc_found")
    code.label("bootsvc_68")
    code.emit_bytes(b"\x49\x8B\x75\x68")           # mov rsi, [r13+0x68]
    code.emit_bytes(b"\x48\x85\xF6")
    emit_jz(code, "bootsvc_70")
    code.emit_bytes(b"\x48\x8B\x86\x28\x00\x00\x00")
    code.emit_bytes(b"\x48\x3D\x00\x00\x10\x00")
    emit_jae(code, "bootsvc_found")
    code.label("bootsvc_70")
    code.emit_bytes(b"\x49\x8B\x75\x70")           # mov rsi, [r13+0x70]
    code.emit_bytes(b"\x48\x85\xF6")
    emit_jz(code, "fail1")
    code.emit_bytes(b"\x48\x8B\x86\x28\x00\x00\x00")
    code.emit_bytes(b"\x48\x3D\x00\x00\x10\x00")
    emit_jae(code, "bootsvc_found")
    code.emit(0xE9)
    code.rel32("fail1")
    code.label("bootsvc_found")
    code.emit_bytes(b"\xB9\x02\x00\x00\x00")       # mov ecx, 2 (AllocateAddress)
    code.emit_bytes(b"\xBA\x01\x00\x00\x00")       # mov edx, 1 (EfiLoaderCode)
    disp = stage_pages_rva - (code.tell() + 7)
    code.emit_bytes(b"\x4C\x8B\x05" + struct.pack("<i", disp))  # mov r8, [rip+stage_pages]
    disp = load_addr_rva - (code.tell() + 7)
    code.emit_bytes(b"\x4C\x8D\x0D" + struct.pack("<i", disp))  # lea r9, [rip+load_addr]
    code.emit_bytes(b"\xFF\xD0")                   # call rax
    code.emit_bytes(b"\x48\x85\xC0")               # test rax, rax
    emit_js(code, "fail2")
    disp = load_addr_rva - (code.tell() + 7)
    code.emit_bytes(b"\x48\x8B\x05" + struct.pack("<i", disp))  # mov rax, [rip+load_addr]
    code.emit_bytes(b"\x48\x3D\x00\x00\x01\x00")  # cmp rax, 0x10000
    emit_jz(code, "load_addr_ok")
    code.emit(0xE9)
    code.rel32("fail2")
    code.label("load_addr_ok")

    code.emit_bytes(b"\x48\x8B\x86\x28\x00\x00\x00")  # mov rax, [rsi+0x28]
    code.emit_bytes(b"\x31\xC9")                   # xor ecx, ecx (AllocateAnyPages)
    code.emit_bytes(b"\xBA\x02\x00\x00\x00")       # mov edx, 2 (EfiLoaderData)
    disp = scratch_pages_rva - (code.tell() + 7)
    code.emit_bytes(b"\x4C\x8B\x05" + struct.pack("<i", disp))  # mov r8, [rip+scratch_pages]
    disp = scratch_alloc_base_rva - (code.tell() + 7)
    code.emit_bytes(b"\x4C\x8D\x0D" + struct.pack("<i", disp))  # lea r9, [rip+scratch_alloc_base]
    code.emit_bytes(b"\xFF\xD0")                   # call rax
    code.emit_bytes(b"\x48\x85\xC0")               # test rax, rax
    emit_js(code, "fail2")

    disp = scratch_alloc_base_rva - (code.tell() + 7)
    code.emit_bytes(b"\x48\x8B\x3D" + struct.pack("<i", disp))  # mov rdi, [rip+scratch_alloc_base]
    disp = scratch_pages_rva - (code.tell() + 7)
    code.emit_bytes(b"\x48\x8B\x0D" + struct.pack("<i", disp))  # mov rcx, [rip+scratch_pages]
    code.emit_bytes(b"\x48\xC1\xE1\x0C")           # shl rcx, 12
    code.emit_bytes(b"\x31\xC0")                   # xor eax, eax
    code.emit_bytes(b"\xF3\xAA")                   # rep stosb

    code.emit_bytes(b"\x48\x8B\x86\x98\x00\x00\x00")  # mov rax, [rsi+0x98] (HandleProtocol)
    code.emit_bytes(b"\x4C\x89\xE1")               # mov rcx, r12
    disp = loaded_guid_rva - (code.tell() + 7)
    code.emit_bytes(b"\x48\x8D\x15" + struct.pack("<i", disp))  # lea rdx, [rip+loaded_guid]
    disp = loaded_ptr_rva - (code.tell() + 7)
    code.emit_bytes(b"\x4C\x8D\x05" + struct.pack("<i", disp))  # lea r8, [rip+loaded_ptr]
    code.emit_bytes(b"\xFF\xD0")                   # call rax
    code.emit_bytes(b"\x48\x85\xC0")               # test rax, rax
    emit_js(code, "fail3")

    disp = loaded_ptr_rva - (code.tell() + 7)
    code.emit_bytes(b"\x48\x8B\x0D" + struct.pack("<i", disp))  # mov rcx, [rip+loaded_ptr]
    code.emit_bytes(b"\x48\x8B\x49\x18")           # mov rcx, [rcx+0x18]
    code.emit_bytes(b"\x48\x8B\x86\x98\x00\x00\x00")  # mov rax, [rsi+0x98] (HandleProtocol)
    disp = block_guid_rva - (code.tell() + 7)
    code.emit_bytes(b"\x48\x8D\x15" + struct.pack("<i", disp))  # lea rdx, [rip+block_guid]
    disp = block_ptr_rva - (code.tell() + 7)
    code.emit_bytes(b"\x4C\x8D\x05" + struct.pack("<i", disp))  # lea r8, [rip+block_ptr]
    code.emit_bytes(b"\xFF\xD0")                   # call rax
    code.emit_bytes(b"\x48\x85\xC0")               # test rax, rax
    emit_js(code, "fail4")

    disp = block_ptr_rva - (code.tell() + 7)
    code.emit_bytes(b"\x48\x8B\x0D" + struct.pack("<i", disp))  # mov rcx, [rip+block_ptr]
    code.emit_bytes(b"\x48\x8B\x41\x08")           # mov rax, [rcx+0x08]
    code.emit_bytes(b"\x8B\x10")                   # mov edx, [rax]
    code.emit_bytes(b"\x49\xB8" + struct.pack("<Q", stage_lba))  # mov r8, lba
    code.emit_bytes(b"\x49\xB9" + struct.pack("<Q", stage_bytes))  # mov r9, bytes
    disp = load_addr_rva - (code.tell() + 7)
    code.emit_bytes(b"\x4C\x8B\x15" + struct.pack("<i", disp))  # mov r10, [rip+load_addr]
    code.emit_bytes(b"\x4C\x89\x54\x24\x20")       # mov [rsp+0x20], r10
    code.emit_bytes(b"\x48\x8B\x41\x18")           # mov rax, [rcx+0x18]
    code.emit_bytes(b"\xFF\xD0")                   # call rax
    code.emit_bytes(b"\x48\x85\xC0")
    emit_js(code, "fail5")

    disp = load_addr_rva - (code.tell() + 7)
    code.emit_bytes(b"\x48\x8B\x1D" + struct.pack("<i", disp))  # mov rbx, [rip+load_addr]
    disp = manifest_offset_rva - (code.tell() + 7)
    code.emit_bytes(b"\x48\x03\x1D" + struct.pack("<i", disp))  # add rbx, [rip+manifest_offset]
    disp = scratch_orig_base_rva - (code.tell() + 7)
    code.emit_bytes(b"\x4C\x8B\x15" + struct.pack("<i", disp))  # mov r10, [rip+scratch_orig_base]
    disp = scratch_alloc_base_rva - (code.tell() + 7)
    code.emit_bytes(b"\x4C\x8B\x1D" + struct.pack("<i", disp))  # mov r11, [rip+scratch_alloc_base]
    disp = scratch_field_count_rva - (code.tell() + 7)
    code.emit_bytes(b"\x48\x8B\x0D" + struct.pack("<i", disp))  # mov rcx, [rip+scratch_field_count]
    disp = scratch_field_offsets_rva - (code.tell() + 7)
    code.emit_bytes(b"\x48\x8D\x35" + struct.pack("<i", disp))  # lea rsi, [rip+scratch_field_offsets]
    code.emit_bytes(b"\x48\x85\xC9")               # test rcx, rcx
    emit_jz(code, "patch_done")
    code.label("patch_loop")
    code.emit_bytes(b"\x8B\x06")                   # mov eax, [rsi]
    code.emit_bytes(b"\x48\x89\xDA")               # mov rdx, rbx
    code.emit_bytes(b"\x48\x01\xC2")               # add rdx, rax
    code.emit_bytes(b"\x4C\x8B\x02")               # mov r8, [rdx]
    code.emit_bytes(b"\x4D\x29\xD0")               # sub r8, r10
    code.emit_bytes(b"\x4D\x01\xD8")               # add r8, r11
    code.emit_bytes(b"\x4C\x89\x02")               # mov [rdx], r8
    code.emit_bytes(b"\x48\x83\xC6\x04")           # add rsi, 4
    code.emit_bytes(b"\x48\xFF\xC9")               # dec rcx
    code.emit(0x75)
    code.rel8("patch_loop")
    code.label("patch_done")

    disp = load_addr_rva - (code.tell() + 7)
    code.emit_bytes(b"\x48\x8B\x05" + struct.pack("<i", disp))  # mov rax, [rip+load_addr]
    code.emit_bytes(b"\x48\x05" + struct.pack("<I", boot_entry_offset))  # add rax, boot_entry_offset
    code.emit_bytes(b"\xFF\xD0")                   # call rax
    code.emit_bytes(b"\x31\xC0")                   # xor eax, eax
    code.emit(0xEB)
    code.rel8("done")
    code.label("fail5")
    code.emit_bytes(b"\xB8\x05\x00\x00\x00")       # mov eax, 5
    code.emit(0xEB)
    code.rel8("done")
    code.label("fail4")
    code.emit_bytes(b"\xB8\x04\x00\x00\x00")       # mov eax, 4
    code.emit(0xEB)
    code.rel8("done")
    code.label("fail3")
    code.emit_bytes(b"\xB8\x03\x00\x00\x00")       # mov eax, 3
    code.emit(0xEB)
    code.rel8("done")
    code.label("fail2")
    code.emit_bytes(b"\xB8\x02\x00\x00\x00")       # mov eax, 2
    code.emit(0xEB)
    code.rel8("done")
    code.label("fail1")
    code.emit_bytes(b"\xB8\x01\x00\x00\x00")       # mov eax, 1
    code.label("done")
    code.emit_bytes(b"\x85\xC0")                   # test eax, eax
    emit_jz(code, "ret")
    code.emit_bytes(b"\x49\x8B\x45\x40")           # mov rax, [r13+0x40]
    code.emit_bytes(b"\x48\x85\xC0")               # test rax, rax
    emit_jz(code, "ret")
    code.emit_bytes(b"\x4C\x8B\x40\x08")           # mov r8, [rax+0x08]
    code.emit_bytes(b"\x4D\x85\xC0")               # test r8, r8
    emit_jz(code, "ret")
    code.emit_bytes(b"\x83\xF8\x02")               # cmp eax, 2
    emit_jz(code, "msg_fail2")
    code.emit_bytes(b"\x83\xF8\x03")               # cmp eax, 3
    emit_jz(code, "msg_fail3")
    code.emit_bytes(b"\x83\xF8\x04")               # cmp eax, 4
    emit_jz(code, "msg_fail4")
    code.emit_bytes(b"\x83\xF8\x05")               # cmp eax, 5
    emit_jz(code, "msg_fail5")
    disp = fail1_rva - (code.tell() + 7)
    code.emit_bytes(b"\x48\x8D\x15" + struct.pack("<i", disp))  # lea rdx, [rip+fail1]
    code.emit(0xEB)
    code.rel8("emit_fail")
    code.label("msg_fail2")
    disp = fail2_rva - (code.tell() + 7)
    code.emit_bytes(b"\x48\x8D\x15" + struct.pack("<i", disp))
    code.emit(0xEB)
    code.rel8("emit_fail")
    code.label("msg_fail3")
    disp = fail3_rva - (code.tell() + 7)
    code.emit_bytes(b"\x48\x8D\x15" + struct.pack("<i", disp))
    code.emit(0xEB)
    code.rel8("emit_fail")
    code.label("msg_fail4")
    disp = fail4_rva - (code.tell() + 7)
    code.emit_bytes(b"\x48\x8D\x15" + struct.pack("<i", disp))
    code.emit(0xEB)
    code.rel8("emit_fail")
    code.label("msg_fail5")
    disp = fail5_rva - (code.tell() + 7)
    code.emit_bytes(b"\x48\x8D\x15" + struct.pack("<i", disp))
    code.label("emit_fail")
    code.emit_bytes(b"\x48\x89\xC1")               # mov rcx, rax
    code.emit_bytes(b"\x41\xFF\xD0")               # call r8
    code.label("ret")
    code.emit_bytes(b"\x48\x83\xC4\x20")           # add rsp, 0x20
    code.emit_bytes(b"\x5F")                       # pop rdi
    code.emit_bytes(b"\x5E")                       # pop rsi
    code.emit_bytes(b"\x5B")                       # pop rbx
    code.emit_bytes(b"\x41\x5D")                   # pop r13
    code.emit_bytes(b"\x41\x5C")                   # pop r12
    code.emit_bytes(b"\xC3")                       # ret
    return code.resolve()


def build_pe(
    stage_lba: int,
    stage_bytes: int,
    boot_entry_offset: int,
    manifest_addr: int,
    scratch_orig_base: int,
    scratch_pages: int,
    scratch_field_offsets: list[int],
) -> bytes:
    message = utf16z("LunaOS UEFI handoff\r\n")
    fail1 = utf16z("LunaOS UEFI fail1\r\n")
    fail2 = utf16z("LunaOS UEFI fail2\r\n")
    fail3 = utf16z("LunaOS UEFI fail3\r\n")
    fail4 = utf16z("LunaOS UEFI fail4\r\n")
    fail5 = utf16z("LunaOS UEFI fail5\r\n")
    loaded_guid = guid_bytes(LOADED_IMAGE_PROTOCOL_GUID)
    block_guid = guid_bytes(BLOCK_IO_PROTOCOL_GUID)
    load_addr = 0x10000
    stage_pages = align_up(stage_bytes, 0x1000) // 0x1000
    loaded_ptr = 0
    block_ptr = 0
    manifest_offset = manifest_addr - load_addr
    data_prefix = struct.pack(
        "<QQQQQQQQQ",
        load_addr,
        stage_pages,
        loaded_ptr,
        block_ptr,
        manifest_offset,
        scratch_orig_base,
        0,
        scratch_pages,
        len(scratch_field_offsets),
    )
    patch_offsets = b"".join(struct.pack("<I", offset) for offset in scratch_field_offsets)
    message_rva = DATA_RVA + len(data_prefix) + len(patch_offsets)
    fail1_rva = message_rva + len(message)
    fail2_rva = fail1_rva + len(fail1)
    fail3_rva = fail2_rva + len(fail2)
    fail4_rva = fail3_rva + len(fail3)
    fail5_rva = fail4_rva + len(fail4)
    loaded_guid_rva = fail5_rva + len(fail5)
    block_guid_rva = loaded_guid_rva + len(loaded_guid)
    data = data_prefix + patch_offsets + message + fail1 + fail2 + fail3 + fail4 + fail5 + loaded_guid + block_guid
    data = data.ljust(align_up(len(data), FILE_ALIGN), b"\x00")

    text = build_text(
        message_rva,
        fail1_rva,
        fail2_rva,
        fail3_rva,
        fail4_rva,
        fail5_rva,
        loaded_guid_rva,
        block_guid_rva,
        DATA_RVA + 16,
        DATA_RVA + 24,
        DATA_RVA,
        DATA_RVA + 8,
        DATA_RVA + 32,
        DATA_RVA + 40,
        DATA_RVA + 48,
        DATA_RVA + 56,
        DATA_RVA + 64,
        DATA_RVA + len(data_prefix),
        stage_lba,
        stage_bytes,
        boot_entry_offset,
    )
    text = text.ljust(align_up(len(text), FILE_ALIGN), b"\x90")

    reloc_entries = struct.pack("<HH", 0xA000, 0x0000)
    reloc = struct.pack("<II", DATA_RVA, 8 + len(reloc_entries)) + reloc_entries
    reloc = reloc.ljust(align_up(len(reloc), FILE_ALIGN), b"\x00")

    text_raw = FILE_ALIGN
    data_raw = text_raw + len(text)
    reloc_raw = data_raw + len(data)

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

    pe += section(b".text", len(text.rstrip(b"\x90")), TEXT_RVA, len(text), text_raw, 0x60000020)
    pe += section(b".data", len(data.rstrip(b"\x00")), DATA_RVA, len(data), data_raw, 0xC0000040)
    pe += section(b".reloc", 12, RELOC_RVA, len(reloc), reloc_raw, 0x42000040)

    headers = (dos + pe).ljust(FILE_ALIGN, b"\x00")
    image = bytearray()
    image += headers
    image += text
    image += data
    image += reloc
    return bytes(image)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--stage-lba", type=int, required=True)
    parser.add_argument("--stage-bytes", type=int, required=True)
    parser.add_argument("--boot-entry-offset", type=int, required=True)
    parser.add_argument("--manifest-addr", type=int, required=True)
    parser.add_argument("--scratch-orig-base", type=int, required=True)
    parser.add_argument("--scratch-pages", type=int, required=True)
    parser.add_argument("--scratch-field-offsets", type=str, required=True)
    args = parser.parse_args()
    scratch_field_offsets = [int(part) for part in args.scratch_field_offsets.split(",") if part]

    OUT.write_bytes(
        build_pe(
            args.stage_lba,
            args.stage_bytes,
            args.boot_entry_offset,
            args.manifest_addr,
            args.scratch_orig_base,
            args.scratch_pages,
            scratch_field_offsets,
        )
    )
    print(OUT)


if __name__ == "__main__":
    main()
