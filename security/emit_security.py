from __future__ import annotations

from asm import Code


TOKEN_LOW = 0x51A7C001
TOKEN_HIGH = 0xC1D00002


def emit_security_space(layout: dict) -> tuple[bytes, dict]:
    code = Code(layout["SPACE"]["SECURITY"])
    def mov_rax_imm(value: int) -> None:
        code.emit(0x48, 0xC7, 0xC0)
        code.emit_u32(value)

    def mov_eax_rdi_disp8(disp: int) -> None:
        code.emit(0x8B, 0x47, disp & 0xFF)

    def cmp_eax_imm32(value: int) -> None:
        code.emit(0x3D)
        code.emit_u32(value)

    def mov_qword_rdi_disp8_rax(disp: int) -> None:
        code.emit(0x48, 0x89, 0x47, disp & 0xFF)

    def mov_dword_rdi_disp8_imm32(disp: int, value: int) -> None:
        code.emit(0xC7, 0x47, disp & 0xFF)
        code.emit_u32(value)

    def ret() -> None:
        code.emit(0xC3)

    code.label("entry_boot")
    code.emit(0x48, 0x8D, 0x35)
    code.rip32("msg_ready")
    code.emit(0xE8)
    code.rel32("print_string")
    ret()

    code.label("entry_gate")
    mov_eax_rdi_disp8(4)
    cmp_eax_imm32(1)
    code.emit(0x74)
    code.rel8("do_request")
    cmp_eax_imm32(2)
    code.emit(0x74)
    code.rel8("do_list")
    mov_dword_rdi_disp8_imm32(40, 0xE001)
    mov_dword_rdi_disp8_imm32(44, 0)
    ret()

    code.label("do_request")
    mov_rax_imm(TOKEN_LOW)
    mov_qword_rdi_disp8_rax(24)
    mov_rax_imm(TOKEN_HIGH)
    mov_qword_rdi_disp8_rax(32)
    mov_dword_rdi_disp8_imm32(40, 0)
    mov_dword_rdi_disp8_imm32(44, 1)
    ret()

    code.label("do_list")
    mov_rax_imm(TOKEN_LOW)
    mov_qword_rdi_disp8_rax(24)
    mov_rax_imm(TOKEN_HIGH)
    mov_qword_rdi_disp8_rax(32)
    mov_dword_rdi_disp8_imm32(40, 0)
    mov_dword_rdi_disp8_imm32(44, 1)
    ret()

    code.label("print_char")
    code.emit(0x88, 0xC4)  # mov ah, al
    code.label("char_wait")
    code.emit(0xBA)
    code.emit_u32(0x3FD)
    code.emit(0xEC)
    code.emit(0xA8, 0x20)
    code.emit(0x74)
    code.rel8("char_wait")
    code.emit(0xBA)
    code.emit_u32(0x3F8)
    code.emit(0x88, 0xE0)
    code.emit(0xEE)
    ret()

    code.label("print_string")
    code.label("string_loop")
    code.emit(0x8A, 0x06)
    code.emit(0x84, 0xC0)
    code.emit(0x74)
    code.rel8("string_done")
    code.emit(0x48, 0xFF, 0xC6)
    code.emit(0xE8)
    code.rel32("print_char")
    code.emit(0xEB)
    code.rel8("string_loop")
    code.label("string_done")
    ret()

    code.label("msg_ready")
    code.emit_bytes(b"[SECURITY] ready\r\n\x00")

    image = code.resolve()
    return image, code.labels
