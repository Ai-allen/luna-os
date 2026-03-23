from __future__ import annotations

from asm import Code


MANIFEST_SECURITY_BOOT = 16
MANIFEST_SECURITY_GATE = 24
MANIFEST_GATE_BASE = 32
MANIFEST_RESERVE_BASE = 40
MANIFEST_RESERVE_SIZE = 48


def emit_boot_space(layout: dict) -> tuple[bytes, dict]:
    code = Code(layout["SPACE"]["BOOT"])

    manifest = layout["MANIFEST"]
    bootview = layout["BOOTVIEW"]
    boot_stack = layout["STACK"]["BOOT"]

    def mov_rsp_imm(value: int) -> None:
        code.emit(0x48, 0xC7, 0xC4)
        code.emit_u32(value)

    def mov_rbx_imm(value: int) -> None:
        code.emit(0x48, 0xC7, 0xC3)
        code.emit_u32(value)

    def mov_rsi_imm(value: int) -> None:
        code.emit(0x48, 0xC7, 0xC6)
        code.emit_u32(value)

    def mov_rdi_imm(value: int) -> None:
        code.emit(0x48, 0xC7, 0xC7)
        code.emit_u32(value)

    def mov_rax_from_rsi_disp8(disp: int) -> None:
        code.emit(0x48, 0x8B, 0x46, disp & 0xFF)

    def mov_rdi_from_rbx() -> None:
        code.emit(0x48, 0x89, 0xDF)

    def mov_qword_rbx_rax() -> None:
        code.emit(0x48, 0x89, 0x03)

    def mov_qword_rbx_disp8_rax(disp: int) -> None:
        code.emit(0x48, 0x89, 0x43, disp & 0xFF)

    def mov_dword_rbx_disp8_imm32(disp: int, value: int) -> None:
        code.emit(0xC7, 0x43, disp & 0xFF)
        code.emit_u32(value)

    def mov_dword_rdi_disp8_imm32(disp: int, value: int) -> None:
        code.emit(0xC7, 0x47, disp & 0xFF)
        code.emit_u32(value)

    def mov_qword_rdi_disp8_imm32(disp: int, value: int) -> None:
        code.emit(0x48, 0xC7, 0x47, disp & 0xFF)
        code.emit_u32(value)

    def mov_eax_rdi_disp8(disp: int) -> None:
        code.emit(0x8B, 0x47, disp & 0xFF)

    def cmp_eax_imm32(value: int) -> None:
        code.emit(0x3D)
        code.emit_u32(value)

    def cmp_dword_rdi_disp8_imm8(disp: int, value: int) -> None:
        code.emit(0x83, 0x7F, disp & 0xFF, value & 0xFF)

    def mov_rax_imm32(value: int) -> None:
        code.emit(0x48, 0xC7, 0xC0)
        code.emit_u32(value)

    def call_rax() -> None:
        code.emit(0xFF, 0xD0)

    def ret() -> None:
        code.emit(0xC3)

    code.label("entry")
    mov_rsp_imm(boot_stack)
    code.emit(0xE8)
    code.rel32("serial_init")
    code.emit(0x48, 0x8D, 0x35)
    code.rip32("msg_boot")
    code.emit(0xE8)
    code.rel32("print_string")

    mov_rbx_imm(bootview)
    mov_rsi_imm(manifest)
    mov_rax_from_rsi_disp8(MANIFEST_GATE_BASE)
    mov_qword_rbx_rax()
    mov_rax_from_rsi_disp8(MANIFEST_RESERVE_BASE)
    mov_qword_rbx_disp8_rax(8)
    mov_rax_from_rsi_disp8(MANIFEST_RESERVE_SIZE)
    mov_qword_rbx_disp8_rax(16)
    mov_dword_rbx_disp8_imm32(24, 0x3F8)

    mov_rax_from_rsi_disp8(MANIFEST_SECURITY_BOOT)
    mov_rdi_from_rbx()
    call_rax()

    mov_rdi_imm(layout["GATE"]["base"])
    mov_dword_rdi_disp8_imm32(0, 1)
    mov_dword_rdi_disp8_imm32(4, 1)
    mov_qword_rdi_disp8_imm32(8, 0)
    mov_qword_rdi_disp8_imm32(16, 0x1101)
    mov_rsi_imm(manifest)
    mov_rax_from_rsi_disp8(MANIFEST_SECURITY_GATE)
    mov_rdi_imm(layout["GATE"]["base"])
    call_rax()
    mov_rdi_imm(layout["GATE"]["base"])
    mov_eax_rdi_disp8(40)
    cmp_eax_imm32(0)
    code.emit(0x75)
    code.rel8("request_fail")
    cmp_dword_rdi_disp8_imm8(44, 1)
    code.emit(0x75)
    code.rel8("request_fail")
    code.emit(0x48, 0x8D, 0x35)
    code.rip32("msg_request_ok")
    code.emit(0xE8)
    code.rel32("print_string")
    code.emit(0xEB)
    code.rel8("request_done")
    code.label("request_fail")
    code.emit(0x48, 0x8D, 0x35)
    code.rip32("msg_request_fail")
    code.emit(0xE8)
    code.rel32("print_string")
    code.label("request_done")

    mov_dword_rdi_disp8_imm32(0, 2)
    mov_dword_rdi_disp8_imm32(4, 2)
    mov_rsi_imm(manifest)
    mov_rax_from_rsi_disp8(MANIFEST_SECURITY_GATE)
    mov_rdi_imm(layout["GATE"]["base"])
    call_rax()
    mov_rdi_imm(layout["GATE"]["base"])
    mov_eax_rdi_disp8(40)
    cmp_eax_imm32(0)
    code.emit(0x75)
    code.rel8("roster_fail")
    cmp_dword_rdi_disp8_imm8(44, 1)
    code.emit(0x75)
    code.rel8("roster_fail")
    code.emit(0x48, 0x8D, 0x35)
    code.rip32("msg_roster_ok")
    code.emit(0xE8)
    code.rel32("print_string")
    code.emit(0xEB)
    code.rel8("halt")
    code.label("roster_fail")
    code.emit(0x48, 0x8D, 0x35)
    code.rip32("msg_roster_fail")
    code.emit(0xE8)
    code.rel32("print_string")

    code.label("halt")
    code.emit(0xF4)
    code.emit(0xEB, 0xFD)

    code.label("serial_init")
    for port, value in (
        (0x3F9, 0x00),
        (0x3FB, 0x80),
        (0x3F8, 0x03),
        (0x3F9, 0x00),
        (0x3FB, 0x03),
        (0x3FA, 0xC7),
        (0x3FC, 0x0B),
    ):
        code.emit(0xBA)
        code.emit_u32(port)
        code.emit(0xB0, value)
        code.emit(0xEE)
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
    code.emit(0x88, 0xE0)  # mov al, ah
    code.emit(0xEE)
    ret()

    code.label("print_string")
    code.label("string_loop")
    code.emit(0x8A, 0x06)  # mov al, [rsi]
    code.emit(0x84, 0xC0)  # test al, al
    code.emit(0x74)
    code.rel8("string_done")
    code.emit(0x48, 0xFF, 0xC6)  # inc rsi
    code.emit(0xE8)
    code.rel32("print_char")
    code.emit(0xEB)
    code.rel8("string_loop")
    code.label("string_done")
    ret()

    code.label("msg_boot")
    code.emit_bytes(b"[BOOT] dawn online\r\n\x00")
    code.label("msg_request_ok")
    code.emit_bytes(b"[BOOT] capability request ok\r\n\x00")
    code.label("msg_request_fail")
    code.emit_bytes(b"[BOOT] capability request fail\r\n\x00")
    code.label("msg_roster_ok")
    code.emit_bytes(b"[BOOT] capability roster ok\r\n\x00")
    code.label("msg_roster_fail")
    code.emit_bytes(b"[BOOT] capability roster fail\r\n\x00")

    image = code.resolve()
    return image, code.labels
